/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "jaxcore_storage.h"
esp_err_t jaxcore_storage_mount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t jaxcore_storage_unmount(void) { return ESP_OK; }
jaxcore_storage_status jaxcore_storage_get_status(void)
{
    return (jaxcore_storage_status) {
        .error = ESP_ERR_NOT_SUPPORTED, .fat_result = -1,
    };
}
const char *jaxcore_storage_stage_name(jaxcore_storage_stage stage)
{
    (void)stage;
    return "disabled";
}
