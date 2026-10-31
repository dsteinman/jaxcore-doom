/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pcm_mixer.h"
#include <math.h>
#include <string.h>

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int doom_pcm_parse_dmx(const void *lump, size_t bytes, doom_pcm_sample *sample)
{
    const uint8_t *p = lump;
    uint32_t count, rate;
    if (!sample) return 0;
    memset(sample, 0, sizeof(*sample));
    if (!p || bytes < 8 || p[0] != 3 || p[1] != 0) return 0;
    rate = (uint32_t)p[2] | ((uint32_t)p[3] << 8);
    count = read_le32(p + 4);
    if (!rate || count <= 48 || count > bytes - 8) return 0;
    sample->data = p + 8 + 16;
    sample->count = count - 32;
    sample->rate = rate;
    return 1;
}

static int clamp(int value, int maximum)
{
    return value < 0 ? 0 : (value > maximum ? maximum : value);
}

static int valid_voice(int voice)
{
    return voice >= 0 && voice < DOOM_MIXER_VOICES;
}

void doom_pcm_init(doom_pcm_mixer *mixer, uint32_t output_rate)
{
    memset(mixer, 0, sizeof(*mixer));
    mixer->rate = output_rate;
}

void doom_pcm_update(doom_pcm_mixer *mixer, int voice, int volume,
                     int separation, int pitch)
{
    doom_pcm_voice *v;
    double step;
    if (!valid_voice(voice) || !mixer->rate) return;
    v = &mixer->voices[voice];
    volume = clamp(volume, 127);
    separation = clamp(separation, 256);
    v->left_gain = volume * (256 - separation);
    v->right_gain = volume * separation;
    step = (double)v->sample.rate * 65536.0 / mixer->rate
         * exp2((clamp(pitch, 255) - 128) / 64.0);
    v->step = step >= UINT32_MAX ? UINT32_MAX
            : (step < 1.0 ? 1 : (uint32_t)step);
}

int doom_pcm_start(doom_pcm_mixer *mixer, int voice,
                   const doom_pcm_sample *sample, int volume, int separation,
                   int pitch)
{
    doom_pcm_voice *v;
    if (!valid_voice(voice) || !mixer->rate || !sample || !sample->data
        || !sample->count || !sample->rate) return 0;
    v = &mixer->voices[voice];
    v->sample = *sample;
    v->position = 0;
    v->active = 1;
    doom_pcm_update(mixer, voice, volume, separation, pitch);
    return 1;
}

void doom_pcm_stop(doom_pcm_mixer *mixer, int voice)
{
    if (valid_voice(voice)) mixer->voices[voice].active = 0;
}

int doom_pcm_playing(const doom_pcm_mixer *mixer, int voice)
{
    return valid_voice(voice) && mixer->voices[voice].active;
}

static int16_t saturate(int64_t sample)
{
    return sample > INT16_MAX ? INT16_MAX
         : (sample < INT16_MIN ? INT16_MIN : (int16_t)sample);
}

void doom_pcm_render(doom_pcm_mixer *mixer, int16_t *out, size_t frames,
                     const int32_t *music)
{
    for (size_t frame = 0; frame < frames; ++frame) {
        int64_t left = music ? music[frame] : 0;
        int64_t right = left;
        for (int i = 0; i < DOOM_MIXER_VOICES; ++i) {
            doom_pcm_voice *v = &mixer->voices[i];
            if (!v->active) continue;
            int32_t pcm = ((int32_t)v->sample.data[v->position >> 16] - 128) * 256;
            left += (int64_t)pcm * v->left_gain / (127 * 256);
            right += (int64_t)pcm * v->right_gain / (127 * 256);
            v->position += v->step;
            if ((v->position >> 16) >= v->sample.count) v->active = 0;
        }
        out[2 * frame] = saturate(left);
        out[2 * frame + 1] = saturate(right);
    }
}
