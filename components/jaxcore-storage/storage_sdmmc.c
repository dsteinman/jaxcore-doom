/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "jaxcore_storage.h"
#include "storage_sdk.h"

static const char *TAG = "sd-nand";
static sdmmc_card_t card;
static FATFS *filesystem;
static BYTE drive_number = FF_DRV_NOT_USED;
static char drive_path[4];
static bool host_live, card_live, disk_live, vfs_live;
static jaxcore_storage_status status = { .fat_result = -1 };

const char *jaxcore_storage_stage_name(jaxcore_storage_stage stage)
{
    switch (stage) {
    case JAXCORE_STORAGE_IDLE: return "idle";
    case JAXCORE_STORAGE_HOST: return "host init";
    case JAXCORE_STORAGE_SLOT: return "slot init";
    case JAXCORE_STORAGE_MEDIA: return "media enumeration";
    case JAXCORE_STORAGE_DRIVE: return "drive allocation";
    case JAXCORE_STORAGE_VFS: return "VFS registration";
    case JAXCORE_STORAGE_FAT: return "FAT mount";
    case JAXCORE_STORAGE_READY: return "ready";
    case JAXCORE_STORAGE_UNMOUNT: return "unmount";
    }
    return "unknown";
}

jaxcore_storage_status jaxcore_storage_get_status(void) { return status; }

/* Unwind only resources acquired here. If teardown fails, retain the remaining
 * ownership flags so a second unmount can finish it without use-after-free. */
static esp_err_t release_resources(void)
{
    if (vfs_live) {
        FRESULT fr = f_mount(NULL, drive_path, 0);
        if (fr != FR_OK) return ESP_FAIL;
        status.mounted = false;
        esp_err_t err = esp_vfs_fat_unregister_path(JAXCORE_STORAGE_MOUNT_POINT);
        if (err != ESP_OK) return err;
        vfs_live = false;
        filesystem = NULL;
    }
    if (disk_live) {
        ff_diskio_unregister(drive_number);
        disk_live = false;
        drive_number = FF_DRV_NOT_USED;
    }
    if (card_live) {
        esp_err_t err = sdmmc_card_deinit(&card);
        if (err != ESP_OK) return err;
        card_live = false;
    }
    if (host_live) {
        esp_err_t err = sdmmc_host_deinit();
        if (err != ESP_OK) return err;
        host_live = false;
    }
    return ESP_OK;
}

esp_err_t jaxcore_storage_unmount(void)
{
    esp_err_t err = release_resources();
    if (err != ESP_OK) {
        status.stage = JAXCORE_STORAGE_UNMOUNT;
        status.error = err;
        status.cleanup_error = err;
        return err;
    }
    status = (jaxcore_storage_status) { .fat_result = -1 };
    return ESP_OK;
}

esp_err_t jaxcore_storage_mount(void)
{
    if (status.cleanup_error != ESP_OK) return ESP_ERR_INVALID_STATE;
    if (status.mounted) return ESP_OK;
    if (host_live || card_live || disk_live || vfs_live)
        return ESP_ERR_INVALID_STATE;
    status = (jaxcore_storage_status) { .fat_result = -1 };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = CONFIG_JAXCORE_STORAGE_MAX_FREQ_KHZ;
    host.io_voltage = 3.3f;
    host.flags &= ~(SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_DDR);
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = JAXCORE_SD_CLK;
    slot.cmd = JAXCORE_SD_CMD;
    slot.d0 = JAXCORE_SD_D0;
    slot.d1 = JAXCORE_SD_D1;
    slot.d2 = JAXCORE_SD_D2;
    slot.d3 = JAXCORE_SD_D3;
    slot.d4 = slot.d5 = slot.d6 = slot.d7 = GPIO_NUM_NC;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    slot.flags = 0; /* External pull-ups; no UHS/1.8 V switching. */
    ESP_LOGI(TAG, "SDMMC slot 0: 4-bit, 3.3 V, max %d kHz; CLK24 CMD25 D0-3=20-23",
             host.max_freq_khz);

    status.stage = JAXCORE_STORAGE_HOST;
    esp_err_t err = sdmmc_host_init();
    if (err != ESP_OK) goto fail;
    host_live = true;
    status.stage = JAXCORE_STORAGE_SLOT;
    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err != ESP_OK) goto fail;
    status.stage = JAXCORE_STORAGE_MEDIA;
    err = sdmmc_card_init(&host, &card);
    if (err != ESP_OK) goto fail; /* pinned SDK frees partial card-init state */
    card_live = true;
    status.capacity_bytes = (uint64_t)card.csd.capacity * card.csd.sector_size;
    sdmmc_card_print_info(stdout, &card);
    ESP_LOGI(TAG, "Reported capacity: %" PRIu64 " bytes", status.capacity_bytes);

    status.stage = JAXCORE_STORAGE_DRIVE;
    err = ff_diskio_get_drive(&drive_number);
    if (err != ESP_OK) goto fail;
    if (drive_number > 9) { err = ESP_ERR_INVALID_STATE; goto fail; }
    snprintf(drive_path, sizeof(drive_path), "%u:", (unsigned)drive_number);
    ff_diskio_register_sdmmc(drive_number, &card);
    disk_live = true;

    status.stage = JAXCORE_STORAGE_VFS;
    esp_vfs_fat_conf_t vfs = {
        .base_path = JAXCORE_STORAGE_MOUNT_POINT,
        .fat_drive = drive_path,
        .max_files = 16,
    };
    err = esp_vfs_fat_register(&vfs, &filesystem);
    if (err != ESP_OK) goto fail; /* an existing registration is not ours */
    vfs_live = true;
    status.stage = JAXCORE_STORAGE_FAT;
    FRESULT fr = f_mount(filesystem, drive_path, 1);
    status.fat_result = (int)fr;
    if (fr != FR_OK) {
        if (fr == FR_NO_FILESYSTEM)
            ESP_LOGE(TAG, "No recognized FAT volume; media left unformatted");
        else
            ESP_LOGE(TAG, "FAT mount failed, FRESULT=%d", (int)fr);
        err = fr == FR_NO_FILESYSTEM ? ESP_ERR_NOT_FOUND : ESP_FAIL;
        goto fail;
    }
    status.stage = JAXCORE_STORAGE_READY;
    status.mounted = true;
    ESP_LOGI(TAG, "Mounted existing FAT volume at %s", JAXCORE_STORAGE_MOUNT_POINT);
    return ESP_OK;

fail:
    status.error = err;
    ESP_LOGE(TAG, "%s failed: %s", jaxcore_storage_stage_name(status.stage),
             esp_err_to_name(err));
    status.cleanup_error = release_resources();
    if (status.cleanup_error != ESP_OK)
        ESP_LOGE(TAG, "Resource cleanup incomplete: %s; unmount before retry",
                 esp_err_to_name(status.cleanup_error));
    return err;
}
