// Copyright 2016-2017 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0
#include <fcntl.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "gamepad.h"
#include "spi_lcd.h"
#include "wad_file.h"
#include "jaxcore_storage.h"
#if CONFIG_DOOM_LEGACY_INPUT && CONFIG_HW_JOYSTICK_OPTION == 2
#include "bt_manager.h"
#include "nvs_flash.h"
#endif

static const char *TAG = "jaxcore";
extern int doom_main(int argc, const char *const *argv);

static void doom_engine_task(void *unused)
{
    (void)unused;
    const char *argv[] = { "doom", "-cout", "ICWEFDA", NULL };
    doom_main(3, argv);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_chip_info_t chip;
    uint32_t flash_bytes = 0;
    esp_chip_info(&chip);
    ESP_ERROR_CHECK(esp_flash_get_size(NULL, &flash_bytes));
    ESP_LOGI(TAG, "target=%s cores=%d revision=%d reset=%d",
             CONFIG_IDF_TARGET, chip.cores, chip.revision, esp_reset_reason());
    ESP_LOGI(TAG, "flash=%lu bytes PSRAM=%u bytes", (unsigned long)flash_bytes,
             (unsigned)esp_psram_get_size());
    ESP_LOGI(TAG, "free internal=%u PSRAM=%u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is unavailable; Doom startup stopped");
        return;
    }
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(TAG, "Heap integrity check failed; Doom startup stopped");
        return;
    }
#if CONFIG_IDF_TARGET_ESP32S31
    ESP_LOGI(TAG, "S31 build bring-up: 320x200 renderer, planned 284x240 ST7789");
    ESP_LOGI(TAG, "display/input disabled; stereo audio uses a software sink");
#endif
#if CONFIG_DOOM_LEGACY_INPUT && CONFIG_HW_JOYSTICK_OPTION == 2
    ESP_ERROR_CHECK(nvs_flash_init());
    bt_init();
#endif
#if CONFIG_JAXCORE_STORAGE_SDMMC
    esp_err_t storage_err = jaxcore_storage_mount();
    if (storage_err != ESP_OK) {
        jaxcore_storage_status storage = jaxcore_storage_get_status();
        ESP_LOGE(TAG, "Storage stopped at %s: %s (FatFS=%d); engine not started",
                 jaxcore_storage_stage_name(storage.stage),
                 esp_err_to_name(storage_err), storage.fat_result);
        return;
    }
#endif
    /* No files or directories are created during startup. */
    const char *required[] = { "DOOM1.WAD", "prboom.wad" };
    for (unsigned i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        int fd = I_Open(required[i], O_RDONLY);
        if (fd < 0) {
            ESP_LOGW(TAG, "WAD unavailable: %s; engine not started", required[i]);
#if CONFIG_DOOM_WAD_FILESYSTEM
            ESP_LOGW(TAG, "Place the required WADs under %s; upload support is pending",
                     CONFIG_DOOM_WAD_ROOT);
#endif
            return;
        }
        I_Close(fd);
    }
    spi_lcd_init();
    jsInit();
    /* Keep stacks internal until PSRAM/cache-off task behavior is validated.
     * ESP-IDF stack sizes are bytes. No static stack in external BSS. */
    ESP_ERROR_CHECK(xTaskCreate(doom_engine_task, "doom", 32768, NULL, 5,
                               NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
