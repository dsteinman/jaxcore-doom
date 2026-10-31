/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "sndhw.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t mutex;
static snd_cb_t render;
static TickType_t period;
static int16_t sink[2 * SND_CHUNK_FRAMES];

static void null_audio_task(void *unused)
{
    (void)unused;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        sndhw_lock();
        render(sink, SND_CHUNK_FRAMES);
        sndhw_unlock();
        xTaskDelayUntil(&wake, period);
    }
}

/* Keep voice lifetimes moving without I2S, codec setup, or audio GPIOs. */
void sndhw_init(int rate, snd_cb_t callback)
{
    configASSERT(rate > 0 && callback && !mutex);
    mutex = xSemaphoreCreateMutex();
    configASSERT(mutex);
    render = callback;
    period = pdMS_TO_TICKS((1000U * SND_CHUNK_FRAMES) / rate);
    if (!period) period = 1;
    ESP_ERROR_CHECK(xTaskCreate(null_audio_task, "audio-null", 4096, NULL, 5,
                               NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void sndhw_lock(void) { if (mutex) xSemaphoreTake(mutex, portMAX_DELAY); }
void sndhw_unlock(void) { if (mutex) xSemaphoreGive(mutex); }
void sndhw_start(void) {}
void sndhw_stop(void) {}
