/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 2001 by
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
 *      Transparent access to data in WADs using mmap
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "doomstat.h"
#include "doomtype.h"

#ifdef __GNUG__
#pragma implementation "w_wad.h"
#endif
#include "w_wad.h"
#include "z_zone.h"
#include "lprintf.h"
#include "i_system.h"

#define RANGECHECK

static struct {
  void *cache;
  void *mmapadr;
#ifdef TIMEDIAG
  int locktic;
#endif
  int locks;
} *cachelump;



#ifdef HEAPDUMP
void W_PrintLump(FILE* fp, void* p) {
  int i;
  for (i=0; i<numlumps; i++)
    if (cachelump[i].cache == p) {
      fprintf(fp, " %8.8s %6u %2d %6d", lumpinfo[i].name,
        W_LumpLength(i), cachelump[i].locks, gametic - cachelump[i].locktic);
      return;
    }
  fprintf(fp, " not found");
}
#endif

#ifdef TIMEDIAG
static void W_ReportLocks(void)
{
  int i;
  lprintf(LO_DEBUG, "W_ReportLocks:\nLump     Size   Locks  Tics\n");
  for (i=0; i<numlumps; i++) {
    if (cachelump[i].locks > 0)
      lprintf(LO_DEBUG, "%8.8s %6u %2d   %6d\n", lumpinfo[i].name,
        W_LumpLength(i), cachelump[i].locks, gametic - cachelump[i].locktic);
  }
}
#endif

void W_InitCache(void)
{
  // set up caching
  cachelump = calloc(numlumps, sizeof *cachelump);
  if (!cachelump)
    I_Error ("W_Init: Couldn't allocate lumpcache");


  /* calloc initializes reference counts to zero. */

#ifdef TIMEDIAG
  atexit(W_ReportLocks);
#endif

}


void W_DoneCache(void)
{
    //ToDo... if we ever unload a wad
}

/* The heap-backed cache follows PrBoom's paired Cache/Unlock contract.
 * Every acquisition pins the lump, including concurrent users of one sound.
 * All cache operations belong to the game task. */
const void* W_CacheLumpNum(int lump)
{
    if ((unsigned)lump >= (unsigned)numlumps)
        I_Error("W_CacheLumpNum: invalid lump %d", lump);
    if (!cachelump[lump].cache) {
        size_t len = W_LumpLength(lump);
        Z_Malloc(len, PU_STATIC, &cachelump[lump].cache);
        if (I_Lseek(lumpinfo[lump].wadfile->handle,
                    lumpinfo[lump].position, SEEK_SET) < 0)
            I_Error("W_CacheLumpNum: cannot seek to lump %d", lump);
        I_Read(lumpinfo[lump].wadfile->handle, cachelump[lump].cache, len);
    } else if (cachelump[lump].locks == 0) {
        Z_ChangeTag(cachelump[lump].cache, PU_STATIC);
    }
    cachelump[lump].locks++;
    return cachelump[lump].cache;
}

const void* W_LockLumpNum(int lump)
{
    return W_CacheLumpNum(lump);
}

void W_UnlockLumpNum(int lump)
{
    if ((unsigned)lump >= (unsigned)numlumps || cachelump[lump].locks <= 0)
        I_Error("W_UnlockLumpNum: unbalanced release of lump %d", lump);
    if (--cachelump[lump].locks == 0)
        Z_ChangeTag(cachelump[lump].cache, PU_CACHE);
}
