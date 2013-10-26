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
// NAME hxnext: serially return (next) record from file.
//
// RETURNS
//  >0    length of record in file, which may exceed (size).
//   0    no more records
//  <0    an error: a HXERR_* enum value.
//
// NOTE: hxnext calls can be mixed with hxput/hxdel in a
//  limited way (qv hxput). hxnext CANNOT be mixed with hxhold,
//  which will cancel hxnext's file lock.
// NOTE: hxnext may not be used with an mmap'ed file.
//  This is just laziness; since hxput currently relies on
//  hxnext's own buffer to be unaffected by actual updates.
//
//  In UPDATE mode, hxnext traverses each chain, and visits
//  shared tails multiple times.
//  In READ mode, hxnext reads each page only once.
//

#include <assert.h>

#include "_hx.h"

static void _hxrel(HXLOCAL *);

//------------------------------------------------------------
int
hxnext(HXFILE * hp, char *rp, int size)
{
    HXLOCAL loc, *locp = &loc;
    HXBUF  *bufp = &hp->buffer;
    char   *recp;

    if (!hp)
        return HXERR_BAD_REQUEST;
    // hxnext(hp,0,0) is technically allowed
    ENTER(locp, hp, NULL, 0);
    locp->mode = hp->mode & HX_UPDATE ? F_WRLCK : F_RDLCK;
    if (SCANNING(hp)) {         // Initialize scan
        _hxsize(locp);
    } else {
        assert(!bufp->page);
        assert(!hp->head);
        assert(!hp->recsize);
        assert(!hp->currpos);

        _hxlock(locp, 0, 0);
        _hxsize(locp);
        if (IS_MMAP(hp))
            _hxremap(locp);
        HOLD_FILE(hp);
        hp->head = locp->npages;
        bufp->page = calloc(1, hp->pgsize);
    }

    hp->currpos += hp->recsize;
    hp->recsize = 0;
    while (1) {

        if (hp->currpos == bufp->used) {
            hp->currpos = 0;

            // Advance to next page
            PAGENO  next = !(hp->mode & HX_UPDATE) ? --hp->head
                : bufp->next ? bufp->next
                : !--hp->head ? 0 : (hp->head -= !IS_HEAD(hp->head));
            if (!next) {
                _hxrel(locp);
                LEAVE(locp, 0);
            }

            if (hp->mmap) {
                HXPAGE *safe = bufp->page;

                _hxload(locp, bufp, next);
                memcpy(safe, bufp->page, sizeof(HXPAGE) + bufp->used);
                bufp->page = safe;
            } else {
                _hxload(locp, bufp, next);
            }

        } else {
            assert(hp->currpos < bufp->used);
            recp = _hxcurrec(hp);

            // In UPDATE mode, only return recs for curr head
            if (!(hp->mode & HX_UPDATE)
                || _hxhead(locp, RECHASH(recp)) == hp->head) {

                hp->recsize = RECSIZE(recp);
                locp->ret = RECLENG(recp);
                memcpy(rp, RECDATA(recp), IMIN(size, locp->ret));
                LEAVE(locp, locp->ret);
            }

            hp->currpos += RECSIZE(recp);
        }
    }
}

int
hxrel(HXFILE * hp)
{
    HXLOCAL loc;

    if (!hp->hold)
        return 0;

    ENTER(&loc, hp, NULL, 0);
    _hxrel(&loc);
    LEAVE(&loc, 0);
}

static void
_hxrel(HXLOCAL * locp)
{
    HXFILE *hp = locp->file;
    HXBUF  *bufp = &hp->buffer;

    locp->mylock = 1;
    hp->hold = 0;

    if (SCANNING(hp)) {
        hp->recsize = hp->currpos = 0;
        free(bufp->page);
        memset(bufp, 0, sizeof *bufp);
    }
}
