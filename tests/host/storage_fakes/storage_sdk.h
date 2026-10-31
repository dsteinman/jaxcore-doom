/* Test-only SDK surface for lifecycle/error injection. Not an ABI substitute:
 * the complete firmware build separately compiles against the real SDK headers. */
#pragma once
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"

#define CONFIG_JAXCORE_STORAGE_MAX_FREQ_KHZ 10000
#define SDMMC_HOST_SLOT_0 0
#define GPIO_NUM_NC -1
#define SDMMC_SLOT_NO_CD GPIO_NUM_NC
#define SDMMC_SLOT_NO_WP GPIO_NUM_NC
#define SDMMC_HOST_FLAG_1BIT 1
#define SDMMC_HOST_FLAG_4BIT 4
#define SDMMC_HOST_FLAG_8BIT 8
#define SDMMC_HOST_FLAG_DDR 16
#define SDMMC_HOST_FLAG_DEINIT_ARG 32
typedef struct { int slot, max_freq_khz, flags; float io_voltage; } sdmmc_host_t;
#define SDMMC_HOST_DEFAULT() { .flags = 1 | 4 | 8 | 16 | 32 }
typedef struct {
    int clk, cmd, d0, d1, d2, d3, d4, d5, d6, d7, cd, wp, width, flags;
} sdmmc_slot_config_t;
#define SDMMC_SLOT_CONFIG_DEFAULT() {0}
typedef struct {
    struct { uint32_t capacity, sector_size; } csd;
} sdmmc_card_t;
typedef unsigned char BYTE;
typedef struct { int marker; } FATFS;
typedef enum { FR_OK = 0, FR_DISK_ERR = 1, FR_NO_FILESYSTEM = 13 } FRESULT;
#define FF_DRV_NOT_USED 0xff
typedef struct {
    const char *base_path, *fat_drive;
    size_t max_files;
} esp_vfs_fat_conf_t;

esp_err_t sdmmc_host_init(void);
esp_err_t sdmmc_host_init_slot(int slot, const sdmmc_slot_config_t *config);
esp_err_t sdmmc_host_deinit(void);
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card);
esp_err_t sdmmc_card_deinit(sdmmc_card_t *card);
void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card);
esp_err_t ff_diskio_get_drive(BYTE *drive);
void ff_diskio_register_sdmmc(BYTE drive, sdmmc_card_t *card);
void ff_diskio_register(BYTE drive, const void *implementation);
#define ff_diskio_unregister(drive) ff_diskio_register(drive, NULL)
esp_err_t esp_vfs_fat_register(const esp_vfs_fat_conf_t *config, FATFS **fs);
esp_err_t esp_vfs_fat_unregister_path(const char *path);
FRESULT f_mount(FATFS *fs, const char *path, BYTE immediate);
void storage_test_log(const char *tag, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
#define ESP_LOGI(...) storage_test_log(__VA_ARGS__)
#define ESP_LOGE(...) storage_test_log(__VA_ARGS__)
