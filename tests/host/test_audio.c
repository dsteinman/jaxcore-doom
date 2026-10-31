/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i_sound.h"
#include "sndhw.h"
#include "w_wad.h"
#include "lprintf.h"

int numChannels = 8;
sfxinfo_t S_sfx[NUMSFX];
static snd_cb_t callback;
static uint8_t lump[8 + 64];
static int pins, in_callback, mutex_depth;

void sndhw_init(int rate, snd_cb_t cb)
{
    assert(rate == 22050); callback = cb;
}
void sndhw_lock(void) { assert(mutex_depth++ == 0); }
void sndhw_unlock(void) { assert(--mutex_depth == 0); }
void sndhw_start(void) {}
void sndhw_stop(void) {}
int (W_CheckNumForName)(const char *name, int ns)
{
    (void)ns; return !strcmp(name, "dspistol") ? 0 : -1;
}
int W_LumpLength(int n) { assert(n == 0); return sizeof(lump); }
const void *W_LockLumpNum(int n)
{
    assert(!in_callback && !mutex_depth && n == 0); ++pins; return lump;
}
void W_UnlockLumpNum(int n)
{
    assert(!in_callback && n == 0 && pins > 0); --pins;
}
int lprintf(OutputLevels level, const char *format, ...)
{
    (void)level; (void)format; return 0;
}
void I_Error(const char *format, ...)
{
    (void)format; abort();
}

static void render(int16_t *out, int frames)
{
    assert(callback && !mutex_depth);
    in_callback = 1; callback(out, frames); in_callback = 0;
}

int main(void)
{
    int16_t out[2 * SND_CHUNK_FRAMES];
    lump[0] = 3; lump[2] = 0x22; lump[3] = 0x56; lump[4] = 64;
    memset(lump + 8, 255, 64);
    S_sfx[sfx_pistol].name = "pistol";
    assert(!I_AnySoundStillPlaying());
    I_StopSound(0); /* safe before initialization */
    I_InitSound();
    I_SetChannels();
    assert(I_GetSfxLumpNum(&S_sfx[sfx_pistol]) == 0);
    assert(I_StartSound(sfx_pistol, 0, 127, 0, 128, 0) == 0);
    assert(I_StartSound(sfx_pistol, 1, 127, 256, 128, 0) == 1);
    assert(pins == 2 && I_AnySoundStillPlaying());
    render(out, 1);
    assert(out[0] == 32512 && out[1] == 32512);
    I_StopSound(1);
    assert(pins == 1);
    I_UpdateSoundParams(0, 127, 256, 128);
    render(out, 1);
    assert(out[0] == 0 && out[1] == 32512);
    render(out, SND_CHUNK_FRAMES);
    assert(pins == 1); /* callback has not touched the cache */
    assert(!I_SoundIsPlaying(0) && pins == 0);
    assert(!I_AnySoundStillPlaying());
    assert(I_StartSound(sfx_pistol, 7, 127, 128, 128, 0) == 7);
    assert(I_StartSound(sfx_pistol, 7, 127, 128, 128, 0) == 7);
    assert(pins == 1); /* replacing the same sound balances both acquisitions */
    lump[0] = 2;
    assert(I_StartSound(sfx_pistol, 7, 127, 128, 128, 0) == -1);
    assert(pins == 1 && I_SoundIsPlaying(7));
    I_StopSound(7); assert(pins == 0);
    assert(I_StartSound(sfx_pistol, 32, 127, 128, 128, 0) == -1);
    assert(!I_SoundIsPlaying(-1));
    lump[0] = 3;
    assert(I_StartSound(sfx_pistol, 0, 127, 128, 128, 0) == 0);
    I_ShutdownSound();
    assert(pins == 0 && !I_AnySoundStillPlaying());
    puts("Actual Doom adapter: stereo, updates, replacement and sample pins passed");
}
