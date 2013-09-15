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
// NAME hxget: retrieve record by key.
//
// DESCRIPTION The record passed in must contain the key value
//  by which the record is to be retrieved.
//
// RETURNS
//  >0	actual length of record (may exceed "size" parameter).
//   0	there is no matching key.
//  <0  HXRET error code.


#include "_hx.h"

// (size < 0) means hxhold: head is left locked on exit.
//  Usual usage is: hxhold(), modify record, hxput().
int
hxget(HXFILE * hp, char *rp, int size)
{
    HXLOCAL	loc, *locp = &loc;
    int		leng, rpos, loops, junk;
    HXBUF       *bufp;
    char        *recp;

    if (!hp || !rp)
	return	HXERR_BAD_REQUEST;
    if (hxdebug > 1) {
        char    buf[25] = {};
        hx_save(hp, rp, size<0 ? -size-1 : size, buf, sizeof buf);
        DEBUG2("%s('%s...')", size < 0 ? "hold" : "get",
                                buf, size);
    }

    ENTER(locp, hp, rp, 1);
    if (size >= 0) {
       locp->mode = F_RDLCK;
        _hxlockset(locp, HEAD_LOCK);    // just lock head
    } else {
        _hxlockset(locp, HIGH_LOCK);    // lock head, +split if > head
        HOLD_HEAD(locp);
    }
    if (IS_MMAP(hp)) _hxremap(locp);

    leng	= 0;
    loops	= HX_MAX_CHAIN;
    bufp	= &locp->buf[0];
    bufp->next	= locp->head;

    do {
        if (!--loops)
    	    LEAVE(locp, HXERR_BAD_FILE);

        _hxload(locp, bufp, bufp->next);
        rpos = _hxfind(locp, bufp, locp->hash, rp, &junk);
    } while (rpos < 0 && bufp->next);

    if (rpos < 0)
    	LEAVE(locp, 0);

    recp = bufp->data + rpos;
    leng = RECLENG(recp);
    memcpy(rp, RECDATA(recp), IMIN(leng, size<0? -size-1 : size));

    LEAVE(locp, leng);
}
