/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef JAXCORE_STORAGE_H
#define JAXCORE_STORAGE_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define JAXCORE_STORAGE_MOUNT_POINT "/sdcard"
/* Dedicated SD-NAND bus from the 2026-10-30 hardware handoff. */
#define JAXCORE_SD_CLK 24
#define JAXCORE_SD_CMD 25
#define JAXCORE_SD_D0 20
#define JAXCORE_SD_D1 21
#define JAXCORE_SD_D2 22
#define JAXCORE_SD_D3 23

typedef enum {
    JAXCORE_STORAGE_IDLE,
    JAXCORE_STORAGE_HOST,
    JAXCORE_STORAGE_SLOT,
    JAXCORE_STORAGE_MEDIA,
    JAXCORE_STORAGE_DRIVE,
    JAXCORE_STORAGE_VFS,
    JAXCORE_STORAGE_FAT,
    JAXCORE_STORAGE_READY,
    JAXCORE_STORAGE_UNMOUNT
} jaxcore_storage_stage;

typedef struct {
    bool mounted;
    jaxcore_storage_stage stage;
    esp_err_t error;
    esp_err_t cleanup_error;
    int fat_result; /* -1 if no mount attempt; otherwise FatFS FRESULT */
    uint64_t capacity_bytes; /* reported by media, not a nominal BOM value */
} jaxcore_storage_status;

/* Startup/control-task APIs; serialize callers. This component exclusively
 * owns the SDMMC controller. It never creates, partitions, or formats a volume.
 * Mount is idempotent. Before unmount, stop Doom/upload activity and close files.
 * Failures release acquired resources; cleanup failures require unmount retry
 * before another mount can proceed. Status preserves the primary failure. */
esp_err_t jaxcore_storage_mount(void);
esp_err_t jaxcore_storage_unmount(void);
jaxcore_storage_status jaxcore_storage_get_status(void);
const char *jaxcore_storage_stage_name(jaxcore_storage_stage stage);
#endif
