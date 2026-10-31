/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include "jaxcore_storage.h"
#include "storage_sdk.h"

enum failure { NONE, HOST, SLOT, MEDIA, DRIVE, VFS, FAT_ABSENT, FAT_IO };
static enum failure fail_at;
static bool host_owned, slot_owned, card_owned, disk_owned, vfs_owned, fat_ready;
static bool fail_fat_unmount, fail_vfs_unmount, fail_host_unmount;
static int host_calls, probe_calls, card_deinit_calls, host_deinit_calls;
static int vfs_unregister_calls;
static BYTE allocated_drive = 3;
static char expected_drive_path[3] = "3:";
static FATFS fs, someone_elses_fs;

const char *esp_err_to_name(esp_err_t error) { (void)error; return "injected error"; }
void storage_test_log(const char *tag, const char *format, ...)
{
    (void)tag; (void)format;
}

esp_err_t sdmmc_host_init(void)
{
    assert(!host_owned); ++host_calls;
    if (fail_at == HOST) return ESP_ERR_NO_MEM;
    host_owned = true;
    return ESP_OK;
}
esp_err_t sdmmc_host_init_slot(int slot, const sdmmc_slot_config_t *config)
{
    assert(host_owned && slot == 0 && config->width == 4);
    assert(config->clk == 24 && config->cmd == 25);
    assert(config->d0 == 20 && config->d1 == 21);
    assert(config->d2 == 22 && config->d3 == 23);
    assert(config->d4 == -1 && config->d5 == -1 && config->d6 == -1 && config->d7 == -1);
    assert(config->cd == -1 && config->wp == -1 && config->flags == 0);
    if (fail_at == SLOT) return ESP_ERR_INVALID_ARG;
    slot_owned = true;
    return ESP_OK;
}
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card)
{
    assert(host_owned && slot_owned); ++probe_calls;
    assert(host->slot == 0 && host->max_freq_khz == 10000 && host->io_voltage == 3.3f);
    assert(!(host->flags & (SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_DDR)));
    assert(host->flags & SDMMC_HOST_FLAG_4BIT);
    if (fail_at == MEDIA) return ESP_ERR_TIMEOUT;
    card->csd.capacity = 1234567; card->csd.sector_size = 512;
    card_owned = true;
    return ESP_OK;
}
esp_err_t sdmmc_card_deinit(sdmmc_card_t *card)
{
    (void)card;
    assert(card_owned && !disk_owned); ++card_deinit_calls;
    card_owned = false;
    return ESP_OK;
}
esp_err_t sdmmc_host_deinit(void)
{
    assert(host_owned && !card_owned); ++host_deinit_calls;
    if (fail_host_unmount) return ESP_FAIL;
    host_owned = slot_owned = false;
    return ESP_OK;
}
void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card)
{
    assert(stream == stdout && card_owned && card->csd.sector_size == 512);
}
esp_err_t ff_diskio_get_drive(BYTE *drive)
{
    assert(card_owned);
    if (fail_at == DRIVE) return ESP_ERR_NOT_FOUND;
    *drive = allocated_drive; /* ensure the implementation does not assume drive zero */
    return ESP_OK;
}
void ff_diskio_register_sdmmc(BYTE drive, sdmmc_card_t *card)
{
    (void)card;
    assert(card_owned && !disk_owned && drive == allocated_drive);
    disk_owned = true;
}
void ff_diskio_register(BYTE drive, const void *implementation)
{
    assert(drive == allocated_drive && disk_owned && !vfs_owned && !implementation);
    disk_owned = false;
}
esp_err_t esp_vfs_fat_register(const esp_vfs_fat_conf_t *config, FATFS **out)
{
    assert(disk_owned && !vfs_owned);
    assert(!strcmp(config->base_path, "/sdcard"));
    assert(!strcmp(config->fat_drive, expected_drive_path) && config->max_files == 16);
    if (fail_at == VFS) {
        *out = &someone_elses_fs;
        return ESP_ERR_INVALID_STATE;
    }
    *out = &fs;
    vfs_owned = true;
    return ESP_OK;
}
esp_err_t esp_vfs_fat_unregister_path(const char *path)
{
    assert(vfs_owned && !fat_ready && !strcmp(path, "/sdcard"));
    ++vfs_unregister_calls;
    if (fail_vfs_unmount) return ESP_FAIL;
    vfs_owned = false;
    return ESP_OK;
}
FRESULT f_mount(FATFS *filesystem, const char *path, BYTE immediate)
{
    assert(disk_owned && vfs_owned && !strcmp(path, expected_drive_path));
    if (!filesystem) {
        assert(immediate == 0);
        if (fail_fat_unmount) return FR_DISK_ERR;
        fat_ready = false;
        return FR_OK;
    }
    assert(filesystem == &fs && immediate == 1);
    if (fail_at == FAT_ABSENT) return FR_NO_FILESYSTEM;
    if (fail_at == FAT_IO) return FR_DISK_ERR;
    fat_ready = true;
    return FR_OK;
}

static void reset(void)
{
    fail_fat_unmount = fail_vfs_unmount = fail_host_unmount = false;
    assert(jaxcore_storage_unmount() == ESP_OK);
    assert(!host_owned && !slot_owned && !card_owned && !disk_owned && !vfs_owned);
    fail_at = NONE;
    allocated_drive = 3;
    expected_drive_path[0] = '3';
    host_calls = probe_calls = card_deinit_calls = host_deinit_calls = 0;
    vfs_unregister_calls = 0;
}

static void test_success(void)
{
    reset();
    assert(jaxcore_storage_mount() == ESP_OK);
    jaxcore_storage_status s = jaxcore_storage_get_status();
    assert(s.mounted && s.stage == JAXCORE_STORAGE_READY && s.error == ESP_OK);
    assert(s.capacity_bytes == (uint64_t)1234567 * 512 && s.fat_result == FR_OK);
    assert(jaxcore_storage_mount() == ESP_OK && host_calls == 1 && probe_calls == 1);
    assert(jaxcore_storage_unmount() == ESP_OK);
    assert(card_deinit_calls == 1 && host_deinit_calls == 1);
    assert(!jaxcore_storage_get_status().mounted);
    assert(jaxcore_storage_unmount() == ESP_OK); /* repeated cleanup is harmless */
}

static void test_failures(void)
{
    const jaxcore_storage_stage stages[] = {
        JAXCORE_STORAGE_IDLE, JAXCORE_STORAGE_HOST, JAXCORE_STORAGE_SLOT,
        JAXCORE_STORAGE_MEDIA, JAXCORE_STORAGE_DRIVE, JAXCORE_STORAGE_VFS,
        JAXCORE_STORAGE_FAT, JAXCORE_STORAGE_FAT,
    };
    for (enum failure failure = HOST; failure <= FAT_IO; ++failure) {
        reset(); fail_at = failure;
        esp_err_t err = jaxcore_storage_mount();
        assert(err != ESP_OK);
        jaxcore_storage_status s = jaxcore_storage_get_status();
        assert(!s.mounted && s.stage == stages[failure] && s.error == err);
        assert(s.cleanup_error == ESP_OK);
        assert(!host_owned && !card_owned && !disk_owned && !vfs_owned);
        assert(host_deinit_calls == (failure != HOST));
        assert(card_deinit_calls == (failure >= DRIVE));
        if (failure == VFS) assert(vfs_unregister_calls == 0); /* don't remove another owner */
        if (failure == FAT_ABSENT) assert(s.fat_result == FR_NO_FILESYSTEM);
        if (failure == FAT_IO) assert(s.fat_result == FR_DISK_ERR);
        fail_at = NONE;
        assert(jaxcore_storage_mount() == ESP_OK); /* a failed mount can be retried */
    }
}

static void test_cleanup_retry(void)
{
    reset(); fail_at = FAT_ABSENT; fail_host_unmount = true;
    assert(jaxcore_storage_mount() == ESP_ERR_NOT_FOUND);
    jaxcore_storage_status s = jaxcore_storage_get_status();
    assert(s.stage == JAXCORE_STORAGE_FAT && s.error == ESP_ERR_NOT_FOUND);
    assert(s.cleanup_error == ESP_FAIL && host_owned && !card_owned);
    assert(jaxcore_storage_mount() == ESP_ERR_INVALID_STATE);
    fail_host_unmount = false;
    assert(jaxcore_storage_unmount() == ESP_OK && !host_owned);

    reset(); assert(jaxcore_storage_mount() == ESP_OK);
    fail_fat_unmount = true;
    assert(jaxcore_storage_unmount() == ESP_FAIL && card_owned && vfs_owned);
    assert(jaxcore_storage_mount() == ESP_ERR_INVALID_STATE);
    fail_fat_unmount = false;
    fail_vfs_unmount = true;
    assert(jaxcore_storage_unmount() == ESP_FAIL && vfs_owned && !fat_ready);
    assert(jaxcore_storage_mount() == ESP_ERR_INVALID_STATE);
    fail_vfs_unmount = false;
    assert(jaxcore_storage_unmount() == ESP_OK && !host_owned);
}

static void test_drive_limits(void)
{
    const BYTE valid[] = { 0, 9 };
    for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        reset();
        allocated_drive = valid[i];
        expected_drive_path[0] = (char)('0' + allocated_drive);
        assert(jaxcore_storage_mount() == ESP_OK);
        assert(jaxcore_storage_unmount() == ESP_OK);
    }
    const BYTE invalid[] = { 10, 255 };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        reset();
        allocated_drive = invalid[i];
        assert(jaxcore_storage_mount() == ESP_ERR_INVALID_STATE);
        jaxcore_storage_status s = jaxcore_storage_get_status();
        assert(s.stage == JAXCORE_STORAGE_DRIVE && s.cleanup_error == ESP_OK);
        assert(!host_owned && !card_owned && !disk_owned && !vfs_owned);
    }
}

int main(void)
{
    test_success(); test_failures(); test_cleanup_retry(); test_drive_limits(); reset();
    puts("Storage: pin configuration, staged failures, ownership and retry passed");
}
