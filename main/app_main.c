// Copyright 2016-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0

#include "esp_attr.h"

#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"

#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#undef false
#undef true
#include "i_system.h"

#include "spi_lcd.h"
#include "sdkconfig.h"
#include "bt_manager.h"

extern void jsInit();

#define JOYSTICK_OPTION CONFIG_HW_JOYSTICK_OPTION

/* ---- Static stack in PSRAM to avoid internal RAM fragmentation ---- */
EXT_RAM_ATTR static StackType_t doomStack[32768];
static StaticTask_t doomTaskBuffer;

void doomEngineTask(void *pvParameters)
{
    printf("doomEngineTask started\n");
    printf("Free heap: %u bytes\n", esp_get_free_heap_size());
    printf("Free internal heap: %u bytes\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    char const *argv[] = {
        "doom",
        "-cout", "ICWEFDA",
        // "nosound",
        // "nomusic",
        // "nosfx",
        NULL
    };

    doom_main(3, argv); //este valor se cambia si se cambian los parametros de argv
}

void app_main()
{
    printf("Reset reason: %d\n", esp_reset_reason());

    /* NVS must be initialized BEFORE starting BLE */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Bluetooth init controlled by Kconfig option */
    if (JOYSTICK_OPTION == 2)
    {
        printf("Bt is enabled. Init\n");
        bt_init();
    }
    else
    {
        printf("Bt is disabled.\n");
    }

    /* Filesystem mode requires the board layer to mount storage before here. */
#if !CONFIG_DOOM_WAD_FILESYSTEM
    /* Partition checks */
    const esp_partition_t *part;

    part = esp_partition_find_first(66, 6, NULL);
    if (part == 0)
        printf("Couldn't find wad part!\n");

    part = esp_partition_find_first(66, 7, NULL);
    if (part == 0)
        printf("Couldn't find prboom wad part!\n");
#endif

    printf("app_main: before spi_lcd_init()\n");
    spi_lcd_init();
    printf("app_main: after spi_lcd_init()\n");

    printf("app_main: before jsInit()\n");
    jsInit();
    printf("app_main: after jsInit()\n");

    printf("app_main: before xTaskCreate()\n");

    printf("Heap before Doom task: %u bytes\n",
           esp_get_free_heap_size());
    printf("Internal heap before Doom task: %u bytes\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    TaskHandle_t doomHandle =
        xTaskCreateStaticPinnedToCore(
            doomEngineTask,
            "doomEngine",
            32768,
            NULL,
            5,
            doomStack,
            &doomTaskBuffer,
            1);

    if (doomHandle == NULL)
    {
        printf("ERROR: Doom static task creation failed!\n");
    }
    else
    {
        printf("Doom static task created successfully\n");
    }

    printf("app_main: after xTaskCreate()\n");

    vTaskDelete(NULL);
}
