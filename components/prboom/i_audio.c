/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  System interface for sound.
 *
 *-----------------------------------------------------------------------------
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"

#include "m_fixed.h"

#include "sndhw.h"

#include "dbopl.h"
#include "pcm_mixer.h"

static int sound_inited;

//PSG: no music no FXsound = 0
int snd_card=1;
int mus_card=1;
#define RATE (22050)
int snd_samplerate=RATE;

static doom_pcm_mixer mixer;
/* Only the game task touches the WAD cache. Finished voices retain their pin
 * until the game task polls, stops, or replaces them; the callback never frees. */
static int sample_lumps[DOOM_MIXER_VOICES];
static int32_t opl_block[SND_CHUNK_FRAMES];

typedef struct {
	uint8_t reg;
	uint8_t data;
	uint16_t delay;
} imf_packet_t;

//imf is 280, 560 or 700Hz tick
#define IMF_RATE 560

typedef struct {
	imf_packet_t *imf;
	int pos;
	int len;
	int delay_to_go;
	Chip opl;
} imf_player_t;

static imf_player_t imfplayer;


static void imf_player_tick(int samps) {
	if (!imfplayer.imf) return;
	while (imfplayer.delay_to_go < samps) {
		//handle next imf packet
		imfplayer.pos++;
		if (imfplayer.pos==imfplayer.len) imfplayer.pos=0;
		Chip__WriteReg(&imfplayer.opl, imfplayer.imf[imfplayer.pos].reg, imfplayer.imf[imfplayer.pos].data);
		samps-=imfplayer.delay_to_go;
		imfplayer.delay_to_go=(imfplayer.imf[imfplayer.pos].delay*RATE)/IMF_RATE;
	}
	imfplayer.delay_to_go-=samps;
}


static void snd_cb(int16_t *buf, int frames)
{
    assert(frames >= 0 && frames <= SND_CHUNK_FRAMES);
    imf_player_tick(frames);
    Chip__GenerateBlock2(&imfplayer.opl, frames, opl_block);
    doom_pcm_render(&mixer, buf, (size_t)frames, opl_block);
}

static int valid_channel(int channel)
{
    return channel >= 0 && channel < DOOM_MIXER_VOICES;
}

/* Caller owns audio mutex; called only on game task. */
static void release_channel(int channel)
{
    doom_pcm_stop(&mixer, channel);
    if (sample_lumps[channel] >= 0) {
        W_UnlockLumpNum(sample_lumps[channel]);
        sample_lumps[channel] = -1;
    }
}

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char name[9];
    snprintf(name, sizeof(name), "ds%.6s", sfx->name);
    return W_CheckNumForName(name);
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int prio)
{
    doom_pcm_sample sample;
    int lump, length;
    const void *data;
    (void)prio;
    if (!sound_inited || !valid_channel(channel) || id <= 0 || id >= NUMSFX)
        return -1;
    lump = I_GetSfxLumpNum(&S_sfx[id]);
    if (lump < 0 || (length = W_LumpLength(lump)) < 0) return -1;

    /* Loading may allocate/read from storage. Do it on the game task before
     * taking the audio mutex, while the old voice's sample remains pinned. */
    data = W_LockLumpNum(lump);
    if (!doom_pcm_parse_dmx(data, (size_t)length, &sample)) {
        W_UnlockLumpNum(lump);
        return -1;
    }
    sndhw_lock();
    release_channel(channel);
    sample_lumps[channel] = lump;
    int started = doom_pcm_start(&mixer, channel, &sample, vol, sep, pitch);
    if (!started) release_channel(channel);
    sndhw_unlock();
    return started ? channel : -1;
}

void I_UpdateSoundParams(int handle, int volume, int separation, int pitch)
{
    if (!sound_inited) return;
    sndhw_lock();
    doom_pcm_update(&mixer, handle, volume, separation, pitch);
    sndhw_unlock();
}

void I_StopSound(int handle)
{
    if (!sound_inited || !valid_channel(handle)) return;
    sndhw_lock();
    release_channel(handle);
    sndhw_unlock();
}

int I_SoundIsPlaying(int handle)
{
    int playing;
    if (!sound_inited || !valid_channel(handle)) return 0;
    sndhw_lock();
    playing = doom_pcm_playing(&mixer, handle);
    if (!playing) release_channel(handle);
    sndhw_unlock();
    return playing;
}

int I_AnySoundStillPlaying(void)
{
    int playing = 0;
    for (int i = 0; i < DOOM_MIXER_VOICES; ++i)
        playing |= I_SoundIsPlaying(i);
    return playing;
}

void I_SetChannels(void)
{
    /* Match PrBoom's configurable channel limit, instead of silently accepting
     * only four of the default eight voices. */
    if (numChannels > DOOM_MIXER_VOICES) numChannels = DOOM_MIXER_VOICES;
    for (int i = 0; i < DOOM_MIXER_VOICES; ++i) I_StopSound(i);
}

void I_ShutdownSound(void)
{
    if (sound_inited) {
        for (int i = 0; i < DOOM_MIXER_VOICES; ++i) I_StopSound(i);
		lprintf(LO_INFO, "I_ShutdownSound: ");
		lprintf(LO_INFO, "\n");
        sound_inited = false;
	}
}

//static SDL_AudioSpec audio;

void I_InitSound(void) {
    doom_pcm_init(&mixer, RATE);
    for (int i = 0; i < DOOM_MIXER_VOICES; ++i) sample_lumps[i] = -1;
	I_InitMusic();
	sndhw_init(RATE, snd_cb);
    sound_inited = true;

	// Finished initialization.
    lprintf(LO_INFO,"I_InitSound: sound ready");
}


void I_InitMusic(void) {
	DBOPL_InitTables();
	Chip__Chip(&imfplayer.opl);
	Chip__Setup(&imfplayer.opl, RATE);
	imfplayer.imf=NULL;
}

void I_PlaySong(uint8_t *data, int len, int looping) {
    if(!data) return;
	
	sndhw_lock();
	imfplayer.imf=(imf_packet_t*)data;
	imfplayer.pos=0;
	imfplayer.len=len/sizeof(imf_packet_t);
	imfplayer.delay_to_go=0;
	sndhw_unlock();
}


void I_PauseSong (int handle){
//	printf("STUB: I_PauseSong %d\n", handle);
}

void I_ResumeSong (void) {
//	printf("STUB: I_ResumeSong %d\n", handle);
}

void I_StopSong(void) {
//	printf("STUB: I_StopSong %d\n", handle);
}

void I_SetMusicVolume(int volume) {
//	printf("STUB: I_SetMusicVolume %d\n", volume);
}
