/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DOOM_WAD_FILE_H
#define DOOM_WAD_FILE_H
#include <stddef.h>
#include <sys/types.h>

/* Read-only WAD access through ESP-IDF VFS or the host filesystem.
 * Mount the filesystem before calling; relative names resolve under root.
 * The engine's existing int file-size API limits individual WADs to INT_MAX.
 * File positions belong to the game task, never the audio callback. */
int doom_wad_open_at(const char *root, const char *name, int flags);
int I_Open(const char *name, int flags);
int I_Lseek(int fd, off_t offset, int whence);
int I_Filelength(int fd);
void I_Read(int fd, void *buffer, size_t bytes);
void I_Close(int fd);
#endif
