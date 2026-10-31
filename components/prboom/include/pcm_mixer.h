/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DOOM_PCM_MIXER_H
#define DOOM_PCM_MIXER_H

#include <stddef.h>
#include <stdint.h>

#define DOOM_MIXER_VOICES 32

typedef struct {
    const uint8_t *data;
    uint32_t count;
    uint32_t rate;
} doom_pcm_sample;

typedef struct {
    doom_pcm_sample sample;
    uint64_t position; /* unsigned 48.16 source-sample position */
    uint32_t step;
    int left_gain;
    int right_gain;
    int active;
} doom_pcm_voice;

typedef struct {
    uint32_t rate;
    doom_pcm_voice voices[DOOM_MIXER_VOICES];
} doom_pcm_mixer;

/* DMX format 3: LE header, unsigned 8-bit PCM, 16 padding bytes at each end.
 * The returned view borrows the lump; the caller must pin it during playback. */
int doom_pcm_parse_dmx(const void *lump, size_t bytes, doom_pcm_sample *sample);
void doom_pcm_init(doom_pcm_mixer *mixer, uint32_t output_rate);
int doom_pcm_start(doom_pcm_mixer *mixer, int voice,
                   const doom_pcm_sample *sample, int volume, int separation,
                   int pitch);
void doom_pcm_update(doom_pcm_mixer *mixer, int voice, int volume,
                     int separation, int pitch);
void doom_pcm_stop(doom_pcm_mixer *mixer, int voice);
int doom_pcm_playing(const doom_pcm_mixer *mixer, int voice);
/* volume: 0..127; separation: 0=left, 128=center, 256=right;
 * pitch: 128=original, +/-64 is one octave. Nearest-neighbor resampling.
 * All functions require external serialization. Rendering allocates nothing.
 * out has 2*frames int16_t elements; optional centered music has frames int32_t
 * elements at unity gain. Saturation happens after all voices and music mix. */
void doom_pcm_render(doom_pcm_mixer *mixer, int16_t *out, size_t frames,
                     const int32_t *music);

#endif
