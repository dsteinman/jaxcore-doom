/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "wad_file.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#define DOOM_WAD_ROOT CONFIG_DOOM_WAD_ROOT
#endif
#ifndef DOOM_WAD_ROOT
#define DOOM_WAD_ROOT "."
#endif

/* Keep this backend independent of engine headers for desktop testing. */
extern void I_Error(const char *format, ...);

int I_Filelength(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    if (!S_ISREG(st.st_mode)) { errno = EINVAL; return -1; }
    if (st.st_size < 0 || st.st_size > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)st.st_size;
}

int doom_wad_open_at(const char *root, const char *name, int flags)
{
    char *path;
    size_t size;
    int fd, saved_errno;
    if (!root || !name || !*name || (flags & O_ACCMODE) != O_RDONLY
        || (flags & (O_CREAT | O_TRUNC | O_APPEND))) {
        errno = EINVAL;
        return -1;
    }
    if (strlen(root) > SIZE_MAX - strlen(name) - 2) {
        errno = ENAMETOOLONG;
        return -1;
    }
    size = strlen(root) + strlen(name) + 2;
    path = malloc(size);
    if (!path) return -1;
    if (name[0] == '/') snprintf(path, size, "%s", name);
    else snprintf(path, size, "%s/%s", root, name);
    fd = open(path, flags);
    saved_errno = errno;
    free(path);
    errno = saved_errno;
    if (fd >= 0 && I_Filelength(fd) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

int I_Open(const char *name, int flags)
{
    return doom_wad_open_at(DOOM_WAD_ROOT, name, flags);
}

int I_Lseek(int fd, off_t offset, int whence)
{
    off_t old = lseek(fd, 0, SEEK_CUR);
    off_t result;
    if (old < 0) return -1;
    result = lseek(fd, offset, whence);
    if (result < 0) return -1;
    if (result > INT_MAX) {
        (void)lseek(fd, old, SEEK_SET);
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

void I_Read(int fd, void *buffer, size_t bytes)
{
    unsigned char *p = buffer;
    while (bytes) {
        size_t chunk = bytes > INT_MAX ? INT_MAX : bytes;
        ssize_t count = read(fd, p, chunk);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            I_Error("I_Read: fd %d: %s (%zu bytes remaining)", fd,
                    count == 0 ? "unexpected end of WAD" : strerror(errno), bytes);
            return;
        }
        p += count;
        bytes -= (size_t)count;
    }
}

void I_Close(int fd)
{
    (void)close(fd);
}
