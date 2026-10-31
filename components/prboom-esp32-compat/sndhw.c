#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "sndhw.h"

// ================= I2S CONFIG =================

#define I2S_PORT   I2S_NUM_0
#define I2S_BCLK   17
#define I2S_WS     47
#define I2S_DOUT   15

#define SND_CHUNKSZ SND_CHUNK_FRAMES

static i2s_chan_handle_t tx_handle = NULL;
static SemaphoreHandle_t audio_mux = NULL;
static snd_cb_t audio_cb = NULL;
static int sample_rate = 22050;

// ============================================================
// AUDIO TASK
// ============================================================

static void audio_task(void *arg)
{
    int16_t *mono_buffer = calloc(SND_CHUNKSZ, sizeof(int16_t));
    int16_t *stereo_buffer = calloc(SND_CHUNKSZ * 2, sizeof(int16_t));

    if (!mono_buffer || !stereo_buffer) {
        printf("Audio buffer alloc failed\n");
        vTaskDelete(NULL);
        return;
    }

    while (1) {

        // The engine callback always generates interleaved stereo frames.
        if (audio_cb) {
            xSemaphoreTake(audio_mux, portMAX_DELAY);
            audio_cb(stereo_buffer, SND_CHUNKSZ);
            xSemaphoreGive(audio_mux);
        } else {
            memset(stereo_buffer, 0, SND_CHUNKSZ * 2 * sizeof(int16_t));
        }

        const void *output = stereo_buffer;
        size_t bytes = SND_CHUNKSZ * 2 * sizeof(int16_t);
#if !CONFIG_DOOM_AUDIO_STEREO
        for (int i = 0; i < SND_CHUNKSZ; ++i)
            mono_buffer[i] = ((int32_t)stereo_buffer[2*i]
                           + stereo_buffer[2*i+1]) / 2;
        output = mono_buffer;
        bytes = SND_CHUNKSZ * sizeof(int16_t);
#endif
        size_t offset = 0;
        while (offset < bytes) {
            size_t written = 0;
            ESP_ERROR_CHECK(i2s_channel_write(tx_handle,
                (const uint8_t *)output + offset, bytes - offset,
                &written, portMAX_DELAY));
            ESP_ERROR_CHECK(written ? ESP_OK : ESP_FAIL);
            offset += written;
        }
    }
}

// ============================================================
// INIT
// ============================================================

void sndhw_init(int rate, snd_cb_t cb)
{
    sample_rate = rate;
    audio_cb = cb;

    // Crear mutex
    audio_mux = xSemaphoreCreateMutex();

    // Crear canal I2S
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);

    chan_cfg.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
#if CONFIG_DOOM_AUDIO_STEREO
            I2S_SLOT_MODE_STEREO
#else
            I2S_SLOT_MODE_MONO
#endif
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = true,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    // Crear tarea de audio
    xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        4096,
        NULL,
        5,
        NULL,
        1
    );

#if CONFIG_DOOM_AUDIO_STEREO
    printf("I2S audio initialized (stereo)\n");
#else
    printf("I2S audio initialized (mono fold-down)\n");
#endif
}

// ============================================================
// LOCK / UNLOCK
// ============================================================

void sndhw_lock(void)
{
    if (audio_mux)
        xSemaphoreTake(audio_mux, portMAX_DELAY);
}

void sndhw_unlock(void)
{
    if (audio_mux)
        xSemaphoreGive(audio_mux);
}

// ============================================================
// START / STOP (no usados actualmente)
// ============================================================

void sndhw_start(void)
{
}

void sndhw_stop(void)
{
}
