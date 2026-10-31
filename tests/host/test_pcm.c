/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pcm_mixer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_dmx(void)
{
    uint8_t lump[8 + 64] = {3, 0, 0x11, 0x2b, 64};
    doom_pcm_sample sample;
    memset(lump + 8, 128, 64);
    assert(doom_pcm_parse_dmx(lump, sizeof(lump), &sample));
    assert(sample.rate == 11025 && sample.count == 32);
    assert(sample.data == lump + 24);
    assert(!doom_pcm_parse_dmx(lump, sizeof(lump) - 1, &sample));
    assert(!sample.data);
    assert(!doom_pcm_parse_dmx(lump, 7, &sample));
    lump[0] = 2;
    assert(!doom_pcm_parse_dmx(lump, sizeof(lump), &sample));
    lump[0] = 3; lump[2] = lump[3] = 0;
    assert(!doom_pcm_parse_dmx(lump, sizeof(lump), &sample));
    lump[2] = 1; lump[4] = 48;
    assert(!doom_pcm_parse_dmx(lump, sizeof(lump), &sample));
    lump[4] = lump[5] = lump[6] = lump[7] = 255;
    assert(!doom_pcm_parse_dmx(lump, sizeof(lump), &sample));
}

static void test_pan_and_end(void)
{
    doom_pcm_mixer mixer;
    uint8_t data[] = {128, 255, 0, 192};
    doom_pcm_sample sample = {data, 4, 22050};
    int16_t out[12];
    doom_pcm_init(&mixer, 22050);
    assert(doom_pcm_start(&mixer, 0, &sample, 127, 0, 128));
    doom_pcm_render(&mixer, out, 1, NULL);
    assert(out[0] == 0 && out[1] == 0); /* unsigned PCM midpoint */
    doom_pcm_render(&mixer, out, 1, NULL);
    assert(out[0] == 32512 && out[1] == 0);
    doom_pcm_update(&mixer, 0, 127, 256, 128);
    doom_pcm_render(&mixer, out, 1, NULL);
    assert(out[0] == 0 && out[1] == -32768);
    doom_pcm_update(&mixer, 0, 127, 128, 128);
    doom_pcm_render(&mixer, out, 3, NULL);
    assert(out[0] == 8192 && out[1] == 8192);
    assert(out[2] == 0 && out[3] == 0 && out[4] == 0 && out[5] == 0);
    assert(!doom_pcm_playing(&mixer, 0));
    assert(!doom_pcm_playing(&mixer, -1));
    assert(!doom_pcm_start(&mixer, 32, &sample, 127, 128, 128));
}

static void test_rate_pitch_and_volume(void)
{
    doom_pcm_mixer mixer;
    uint8_t data[] = {192, 160, 128, 96};
    doom_pcm_sample sample = {data, 4, 11025};
    int16_t out[16];
    doom_pcm_init(&mixer, 22050);
    assert(doom_pcm_start(&mixer, 0, &sample, 127, 0, 128));
    doom_pcm_render(&mixer, out, 8, NULL);
    for (int i = 0; i < 4; ++i) {
        assert(out[4*i] == ((int)data[i] - 128) * 256);
        assert(out[4*i] == out[4*i+2]);
    }
    assert(!doom_pcm_playing(&mixer, 0));
    assert(doom_pcm_start(&mixer, 0, &sample, 127, 0, 192));
    doom_pcm_render(&mixer, out, 4, NULL);
    for (int i = 0; i < 4; ++i)
        assert(out[2*i] == ((int)data[i] - 128) * 256);
    assert(!doom_pcm_playing(&mixer, 0));
    assert(doom_pcm_start(&mixer, 0, &sample, 0, 128, 128));
    doom_pcm_render(&mixer, out, 1, NULL);
    assert(out[0] == 0 && out[1] == 0);
    doom_pcm_stop(&mixer, 0);
    assert(!doom_pcm_playing(&mixer, 0));
    doom_pcm_init(&mixer, 0);
    assert(!doom_pcm_start(&mixer, 0, &sample, 127, 0, 128));
}

static void test_polyphony_and_clipping(void)
{
    doom_pcm_mixer mixer;
    uint8_t positive[] = {255, 255}, negative[] = {0, 0};
    doom_pcm_sample a = {positive, 2, 48000}, b = {negative, 2, 48000};
    int32_t music[] = {1000, -1000};
    int16_t out[4];
    doom_pcm_init(&mixer, 48000);
    for (int i = 0; i < DOOM_MIXER_VOICES; ++i)
        assert(doom_pcm_start(&mixer, i, i % 2 ? &b : &a, 127,
                              i % 2 ? 256 : 0, 128));
    doom_pcm_render(&mixer, out, 2, music);
    assert(out[0] == INT16_MAX && out[1] == INT16_MIN);
    assert(out[2] == INT16_MAX && out[3] == INT16_MIN);
    doom_pcm_render(&mixer, out, 2, music);
    assert(out[0] == 1000 && out[1] == 1000);
    assert(out[2] == -1000 && out[3] == -1000);
}

int main(void)
{
    test_dmx(); test_pan_and_end(); test_rate_pitch_and_volume();
    test_polyphony_and_clipping();
    puts("DMX bounds, stereo, pitch, resampling, 32 voices and clipping passed");
}
