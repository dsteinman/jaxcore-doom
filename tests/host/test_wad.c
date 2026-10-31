/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "wad_file.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static jmp_buf fatal;
static int expect_error;
void I_Error(const char *format, ...)
{
    (void)format;
    assert(expect_error);
    longjmp(fatal, 1);
}

int main(void)
{
    char root[] = "/tmp/jaxcore-wad-XXXXXX", path[128], data[16] = {0};
    assert(mkdtemp(root));
    snprintf(path, sizeof(path), "%s/test.wad", root);
    int writer = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(writer >= 0);
    assert(write(writer, "IWAD01234567", 12) == 12);
    close(writer);
    int fd = doom_wad_open_at(root, "test.wad", O_RDONLY);
    assert(fd >= 0 && I_Filelength(fd) == 12);
    I_Read(fd, data, 4);
    assert(!memcmp(data, "IWAD", 4));
    I_Read(fd, data, 4);
    assert(!memcmp(data, "0123", 4)); /* reads advance position */
    assert(I_Lseek(fd, -2, SEEK_CUR) == 6);
    assert(I_Lseek(fd, -4, SEEK_END) == 8);
    I_Read(fd, data, 4);
    assert(!memcmp(data, "4567", 4));
    I_Read(fd, data, 0);
    expect_error = 1;
    if (!setjmp(fatal)) { I_Read(fd, data, 1); assert(!"EOF must fail"); }
    expect_error = 0;
    assert(I_Lseek(fd, -1, SEEK_SET) == -1);
    if (sizeof(off_t) > 4) {
        assert(I_Lseek(fd, (off_t)INT_MAX + 1, SEEK_SET) == -1);
        assert(errno == EOVERFLOW && I_Lseek(fd, 0, SEEK_CUR) == 12);
    }
    I_Close(fd);
    assert(I_Filelength(fd) == -1);
    assert(doom_wad_open_at(root, "missing.wad", O_RDONLY) == -1);
    assert(doom_wad_open_at(root, "test.wad", O_WRONLY) == -1);
    assert(doom_wad_open_at(root, "test.wad", O_RDONLY | O_TRUNC) == -1);
    assert(doom_wad_open_at(root, "", O_RDONLY) == -1);
    assert(doom_wad_open_at(root, ".", O_RDONLY) == -1);
    fd = doom_wad_open_at("/unused", path, O_RDONLY);
    assert(fd >= 0); I_Close(fd);
    /* Exercise I_Open's configured default root via the actual working dir. */
    int cwd = open(".", O_RDONLY);
    assert(cwd >= 0 && chdir(root) == 0);
    fd = I_Open("test.wad", O_RDONLY);
    assert(fd >= 0); I_Close(fd);
    assert(fchdir(cwd) == 0); close(cwd);
    if (sizeof(off_t) > 4) {
        writer = open(path, O_WRONLY);
        assert(writer >= 0 && ftruncate(writer, (off_t)INT_MAX + 1) == 0);
        close(writer);
        assert(doom_wad_open_at(root, "test.wad", O_RDONLY) == -1);
        assert(errno == EOVERFLOW);
    }
    assert(unlink(path) == 0 && rmdir(root) == 0);
    puts("WAD roots, sequential reads, seek, EOF and size bounds passed");
}
