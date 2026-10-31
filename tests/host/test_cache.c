/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "doomtype.h"
#include "w_wad.h"
#include "wad_file.h"
#include "lprintf.h"
#include "z_zone.h"
#undef malloc
#undef calloc
#undef free
#undef realloc
#undef strdup

int numlumps = 1;
static lumpinfo_t info;
lumpinfo_t *lumpinfo = &info;
static void *cache_metadata, *allocation;
static void **owner;
static int current_tag;
extern void W_InitCache(void);

void *(Z_Malloc)(size_t bytes, int tag, void **user)
{
    assert(!allocation && user);
    allocation = malloc(bytes); assert(allocation);
    owner = user; *user = allocation; current_tag = tag;
    return allocation;
}
void *(Z_Calloc)(size_t count, size_t bytes, int tag, void **user)
{
    (void)tag; (void)user;
    assert(!cache_metadata);
    cache_metadata = calloc(count, bytes); assert(cache_metadata);
    return cache_metadata;
}
void (Z_ChangeTag)(void *data, int tag)
{
    assert(data == allocation); current_tag = tag;
}
int lprintf(OutputLevels level, const char *format, ...)
{
    (void)level; (void)format; return 0;
}
void I_Error(const char *format, ...)
{
    (void)format; abort();
}
int W_LumpLength(int n) { assert(n == 0); return info.size; }

int main(void)
{
    char path[] = "/tmp/jaxcore-cache-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    assert(write(fd, "asset", 5) == 5);
    wadfile_info_t wad = {0};
    wad.handle = fd; info.wadfile = &wad; info.size = 5;
    W_InitCache();
    const void *a = W_CacheLumpNum(0);
    assert(!memcmp(a, "asset", 5) && current_tag == PU_STATIC);
    const void *b = W_LockLumpNum(0);
    assert(b == a);
    W_UnlockLumpNum(0);
    assert(current_tag == PU_STATIC); /* second user still owns the data */
    W_UnlockLumpNum(0);
    assert(current_tag == PU_CACHE);
    /* Simulate zone eviction and its owner-pointer clearing. */
    free(allocation); allocation = NULL; *owner = NULL;
    a = W_LockLumpNum(0);
    assert(!memcmp(a, "asset", 5) && current_tag == PU_STATIC);
    W_UnlockLumpNum(0);
    assert(current_tag == PU_CACHE);
    free(allocation); free(cache_metadata);
    I_Close(fd); assert(unlink(path) == 0);
    puts("Actual WAD cache: shared pins, final release and eviction/reload passed");
}
