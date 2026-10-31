#ifndef DOOM_SNDHW_H
#define DOOM_SNDHW_H
#include <stdint.h>

/* frames counts L/R pairs; buf contains 2 * frames signed int16_t samples.
 * No allocation, file reads, or WAD cache operations in this callback. */
#define SND_CHUNK_FRAMES 560
typedef void (*snd_cb_t)(int16_t *buf, int frames);

void sndhw_lock();
void sndhw_unlock();
void sndhw_init(int rate, snd_cb_t cb);
#endif
