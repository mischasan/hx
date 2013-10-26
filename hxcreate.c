// Copyright (C) 2001-2013 Mischa Sandberg <mischasan@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 2 as
// published by the Free Software Foundation.  You may not use, modify or
// distribute this program under any other version of the GNU General
// Public License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  IF YOU HAVE NO WAY OF WORKING WITH GPL, CONTACT ME.
//-------------------------------------------------------------------------------
// NAME int hxcreate(name, mode, pgsize, udata, uleng)
//
// DESCRIPTION "hxcreate" initializes a file as a HX file.
//
// "pgsize" is the block size for this file. This is rounded up
//  to a power of two not smaller than the physical block size
//  of the file system. No record can be larger than
//  (pgsize - 12) bytes.
//
// "udata" points to an array of (uleng) bytes to be written
//  into the head of the file, and which is retrieved by
//  "hxopen". It can be used to include "file type" information.
//  (udata,uleng) are passed to every hash() and diff().
//
// RETURNS
//   0    success
//  -1    file error (see "errno"); or file not empty.

#include <fcntl.h>
#include <sys/stat.h>

#include "_hx.h"

#define CREATE    (O_CREAT|O_RDWR|O_BINARY|O_TRUNC)

HXRET
hxcreate(char const *name, int mode, int pgsize, char const *udata, int uleng)
{
    if (pgsize && (pgsize < HX_MIN_PGSIZE || pgsize & (pgsize - 1)))
        return HXERR_BAD_REQUEST;

    int     fd = open(name, CREATE, mode & 0777);

    if (fd < 0)
        return HXERR_CREATE;
    if (!pgsize) {
        struct stat sb;

        if (fstat(fd, &sb))
            return HXERR_CREATE;
        pgsize = sb.st_blksize;
    }

    HXPAGE *pp = (HXPAGE *) calloc(1, pgsize);
    HXROOT *rp = (HXROOT *) pp;
    char   *dp = (char *)(rp + 1);

    if (uleng >= (int)(pgsize - sizeof(HXROOT))) {
        unlink(name);
        return HXERR_BAD_REQUEST;
    }

    STSH(pgsize, &rp->pgsize);
    STSH(HXVERSION, &rp->version);
    STSH(uleng, &rp->uleng);

    int     ret = HXOKAY;

    memcpy(dp, udata, uleng);
    dp[uleng] = 0x01;           // allocate page 0 (this page)
    if (pgsize != write(fd, (char *)rp, pgsize))
        ret = -1;
    memset((char *)pp, 0, pgsize);
    pp->next = 0;
    if (pgsize != write(fd, (char *)pp, pgsize))
        ret = -1;

    free(pp);
    close(fd);

    return ret;
}
