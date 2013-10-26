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
// hxshape: adjust the density of a file, to match a desired
//  average number of (overflow) page loads per unsuccessful
//  query.
// There are two basic tests for efficiency of a hash table:
// avg number of disk reads per successful or unsuccessful read.
// The former is always less than the latter. The former also
// depends on how many records are in the overflow blocks,
// which makes it an iffier metric.
//
// hxshape(hp, x) either expands or compresses the file,
// attempting to achieve an average of (x) overflow reads per
// unsuccessful lookup. "x" should be in the range 0 .. 1,
// though it is technically (barely) possible to have a file
// with X > 1. Note that hxshape only ATTEMPTS to reach (x);
// when a file is packed as dense as possible, its overflow
// stat is still less than 1 (typically, 0.6)
//
// hxshape always begins by combining tail pages, then decides
//  whether to expand or contract the file.
//
// Expansion does repeated _hxgrow calls.
//  _hxgrow is code ripped out of hxput, so it always
//  ensures that the page (buffer) it returns is marked as
//  allocated in the bitmap ... which hxshape immediately
//  frees with _hxalloc(..., 0). There's a more efficient
//  way to do this (qv hxbuild updating map pages).
//
// Compression reduces the size of a hx file, by:
//  - moving overflow pages into lower (empty) slots
//  - merging split head pages
//  - truncating trailing (empty) pages of file.
//
// NOTES Merging chains can alter the contents of tail pages,
//  which means a second hxpack will recover a bit more space
//  than the first.

#include <assert.h>

#include "_hx.h"

static int
cmpused(PGINFO const *a, PGINFO const *b)
{
    return (int)a->used - (int)b->used;
}

//--------------|---------------------------------------------
HXRET
hxshape(HXFILE * hp, double overload)
{
    HXLOCAL loc, *locp = &loc;
    HXBUF  *srcp, *dstp, *oldp;
    int     pos, bitpos;
    PGINFO *atail, *ztail;
    PAGENO  pg, pm, *aprev, *afree, *zfree;
    double  totbytes = 0, fullbytes = 0, fullpages = 0;

    if (!hp || hp->buffer.pgno || hp->mode & HX_MMAP
        || !(hp->mode & HX_UPDATE))
        return HXERR_BAD_REQUEST;

    ENTER(locp, hp, NULL, 3);

    _hxlock(locp, 0, 0);
    _hxsize(locp);

    srcp = &locp->buf[0];
    dstp = &locp->buf[1];
    oldp = &locp->buf[2];

    _hxinitRefs(locp);

    // Populate vnext,vrefs,vtail for tail-merging:
    ztail = calloc(locp->npages / HXPGRATE + 1, sizeof(PGINFO));
    locp->vtail = ztail;

    for (pg = 1; pg < locp->npages; ++pg) {
        PGINFO  x = _hxpginfo(locp, pg);

        _hxsetRef(locp, pg, x.pgno);
        totbytes += x.used;
        if (x.pgno)             // i.e. page.next != 0
            ++fullpages, fullbytes += x.used;
        else if (x.used && !IS_HEAD(pg))
            x.pgno = pg, *ztail++ = x;
    }

    // Sort vtail by (used), so that smallest+largest (used)
    // counts can be matched up; simple greedy-fill algorithm.
    qsort(locp->vtail, ztail - locp->vtail,
          sizeof *locp->vtail, (cmpfn_t) cmpused);

    // Combine tail pages where possible:
    for (atail = locp->vtail, --ztail; atail < ztail;) {
        if (!_FITS(hp, atail->used, atail->recs, ztail->used, ztail->recs)) {
            --ztail;
            continue;
        }
        // Merge is always from [atail] to [ztail], to maintain
        // ([ztail].used >= [ztail-1].used).
        if (atail->pgno < ztail->pgno) {
            PGINFO  tmp = *atail;

            *atail = *ztail;
            *ztail = tmp;
        }

        _hxload(locp, srcp, atail->pgno);
        _hxload(locp, dstp, ztail->pgno);
        _hxappend(dstp, srcp->data, srcp->used);
        dstp->recs += srcp->recs;
        _hxsave(locp, dstp);
        _hxfindRefs(locp, srcp, srcp->pgno);

        for (aprev = locp->vprev; *aprev; ++aprev)
            PUTLINK(locp, *aprev, dstp->pgno);

        ztail->used += srcp->used;
        srcp->used = srcp->recs = 0;
        BUFLINK(locp, srcp, 0);
        _hxsave(locp, srcp);
        _hxalloc(locp, srcp->pgno, 0);
        ++atail;
    }

    // Now decide whether to grow or shrink the file.

    PAGENO  overflows = 0;

    for (pg = 1; pg < locp->npages; ++pg) {
        if (IS_HEAD(pg)) {
            int     loops = HX_MAX_CHAIN;

            for (pm = pg; (pm = locp->vnext[pm]); ++overflows)
                if (!--loops)
                    LEAVE(locp, HXERR_BAD_FILE);
        }
    }

    PAGENO  dpages = locp->dpages + overflows;
    PAGENO  goodsize = dpages / (1.0 + overload);

    DEBUG("%.0f/%.0f=%0.f  %.0f %lu/%.2f=%lu => %lu",
          fullbytes, fullpages, fullbytes / fullpages,
          totbytes, dpages, overload + 1, goodsize, _hxd2f(goodsize));
    // "+1" for the root page
    goodsize = goodsize ? _hxd2f(goodsize) + 1 : 2;
    if (locp->npages <= goodsize) {
        // Increase dpages.
        // Note that _hxgrow always returns an ALLOCATED
        //  overflow. It would be smarter to clear all the
        //  map bits in one step at the end, the way
        //  hxbuild sets all the map bits in one load/save.
        PAGENO  junk = 0;

        while (locp->npages < goodsize) {
            _hxgrow(locp, dstp, DATASIZE(hp), &junk);
            _hxsave(locp, dstp);
            _hxalloc(locp, dstp->pgno, 0);
        }

        _hxflushfreed(locp, dstp);
        LEAVE(locp, HXNOTE);
    }
    // Build a list of free pgnos
    assert(sizeof *afree <= sizeof *locp->vtail);
    afree = zfree = (PAGENO *) locp->vtail;

    for (pg = pm = 0; pg < locp->npages; pg += HXPGRATE) {
        if (!VREF(locp, pg) && !IS_MAP(hp, pg))
            *zfree++ = pg;
    }

    // Since we are decrementing npages BEFORE reading last page,
    //  set locked such that _hxislocked gives correct answer.
    hp->locked |= LOCKED_BEYOND;
    // Work backward from end of file, trimming pages.
    for (; locp->npages > goodsize; --locp->npages) {

        PAGENO  srchead = locp->npages - 1;
        PAGENO  srctail, dsthead, dstneck, dsttail;
        int     loops = HX_MAX_CHAIN;

        // EASIEST CASE: a map or unreferenced oveflow page
        if (srchead == zfree[-1] || IS_MAP(hp, srchead)) {
            assert(!VREF(locp, srchead) && !IS_HEAD(srchead));
            --zfree;
            continue;
        }
        // EASIER CASE: an empty head page
        _hxload(locp, srcp, srchead);
        if (!srcp->used)
            continue;

        // Anything from here on might need 2 free pages
        if (zfree - afree < 2)
            break;

        --VREF(locp, srcp->next);

        STAIN(srcp);
        srcp->pgno = *afree++;
#   if 0
        // hxshape does not work with MMAP as yet.
        if (hp->mmap)
            memcpy(&hp->mmap[(off_t) srcp->pgno * hp->pgsize],
                   srcp->page, hp->pgsize);
#   endif
        _hxalloc(locp, srcp->pgno, 1);
        _hxsetRef(locp, srcp->pgno, srcp->next);

        // EASY CASE: an overflow page to relocate:
        if (!IS_HEAD(srchead)) {

            _hxsave(locp, srcp);
            _hxfindRefs(locp, srcp, srchead);

            for (aprev = locp->vprev; *aprev; ++aprev)
                PUTLINK(locp, *aprev, srcp->pgno);

            continue;
        }
        // HARD CASE: a head page to desplit:
        locp->hash = RECHASH(srcp->data);
        _hxpoint(locp);         // recalc (dpages,head)

        dsthead = locp->head;
        dstneck = locp->vnext[dsthead];
        dsttail = _hxgetRef(locp, dsthead, 0);
        srctail = _hxgetRef(locp, srchead, 0);

        // Append srchead to dsttail, or insert chain between
        // dsthead and vnext[dsthead]. If srctail is shared,
        // make a copy of * it first.
        if (dsttail == dsthead || VREF(locp, dsttail) == 1) {

            dsthead = dsttail;

        } else if (srctail == srchead) {

            BUFLINK(locp, srcp, dstneck);

        } else if (VREF(locp, srctail) == 1) {

            PUTLINK(locp, srctail, dstneck);

        } else if (srctail == dsttail) {

            if (dsttail != dstneck) {

                srctail = _hxgetRef(locp, srcp->pgno, srctail);

                if (srctail == srcp->pgno) {

                    BUFLINK(locp, srcp, dstneck);

                } else {

                    PUTLINK(locp, srctail, dstneck);
                }
            }

        } else {

            _hxload(locp, oldp, srctail);
            if (!oldp->used)
                LEAVE(locp, HXERR_BAD_FILE);

            _hxfresh(locp, dstp, *afree++);
            _hxalloc(locp, dstp->pgno, 1);
            _hxshift(locp, locp->head, srchead, oldp, dstp, dstp);
            _hxsave(locp, oldp);
            // This hack prevents the CHECK in _hxlink from
            // aborting when oldp contains a page that the
            // next iteration wants to PUTLINK. This ONLY occurs
            // in this code (I think).
            // TODO: can this happen in (hxfix,hxshape)??
            oldp->pgno = -1;

            BUFLINK(locp, dstp, dstneck);
            _hxsave(locp, dstp);

            if (srcp->next == srctail) {

                BUFLINK(locp, srcp, dstp->pgno);

            } else {

                pg = _hxgetRef(locp, srchead, srctail);
                PUTLINK(locp, pg, dstp->pgno);
            }
        }

        _hxload(locp, dstp, dsthead);
        BUFLINK(locp, dstp, srcp->pgno);

        // Cannot early-out on !SHRUNK here (as "hxput" does);
        //  new chain may have vacancies in two places.
        while (1) {

            if (!--loops)
                LEAVE(locp, HXERR_BAD_FILE);

            if (_hxshift(locp, locp->head, srchead, srcp, dstp, dstp)) {

                SWAP(srcp, dstp);

            } else {

                BUFLINK(locp, dstp, srcp->next);

                if (!srcp->used != !VREF(locp, srcp->pgno))
                    LEAVE(locp, HXERR_BAD_FILE);

                if (!srcp->used) {
                    BUFLINK(locp, srcp, 0);
                    *--afree = srcp->pgno;
                    _hxalloc(locp, srcp->pgno, 0);
                }
            }

            _hxsave(locp, srcp);
            if (!dstp->next)
                break;

            _hxload(locp, srcp, dstp->next);
        }

        _hxsave(locp, dstp);
    }

    // npages was overdecremented by one in loop
    _hxresize(locp, locp->npages + 1);

    // Zero the freemap for all truncated overflow pages:
    pg = _hxmap(hp, locp->npages + HXPGRATE
                - locp->npages % HXPGRATE, &bitpos);

    _hxload(locp, dstp, pg);
    DEBUG2("clear map %lu from bit %d onward", pg, bitpos);
    pos = bitpos >> 3;
    dstp->data[pos++] &= ~(-1 << (bitpos & 7));
    memset(dstp->data + pos, 0, DATASIZE(hp) - pos);
    STAIN(dstp);
    _hxsave(locp, dstp);

    LEAVE(locp, HXOKAY);
}

//EOF
