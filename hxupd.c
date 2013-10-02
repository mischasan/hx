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
// hxupd: utilities for implementing HX file updates.

#include <assert.h>
#include <errno.h>
#include <stdint.h>             // uintptr_t
#include <stdarg.h>

#include "_hx.h"

static void _write(HXLOCAL *, off_t, void *, int);

// _hxalloc: mark an overflow page in the bitmap as used/free.
void
_hxalloc(HXLOCAL * locp, PAGENO pgno, int bitval)
{
    HXFILE *hp = locp->file;
    int     bitpos;
    off_t   pos = _hxmap(hp, pgno, &bitpos);
    BYTE    bits, newbit = 1 << (bitpos & 7);

    DEBUG3("pgno=%u set=%d mpg=%llu bit=%d", pgno, bitval, pos, bitpos);

    //_hxgrow uses _hxalloc to alloc new map pages.
    assert(bitval || !IS_MAP(hp, pgno));
    assert(!IS_HEAD(pgno));
    _hxlock(locp, pos, 1);

    pos = pos * hp->pgsize + sizeof(HXPAGE) + (bitpos >> 3);
    if (hp->mmap) {
        bits = hp->mmap[pos];
    } else {
        _hxread(locp, pos, &bits, 1);
    }
    DEBUG3("byte %2.2X %c %2.2X%s", bits, "-+"[bitval], newbit, !bitval == !(bits & newbit) ? " ERROR" : "");
    if (!bitval == !(bits & newbit))
        LEAVE(locp, HXERR_BAD_FILE);

    // bits ^= newbit;
    if (bitval)
        bits |= newbit;
    else
        bits &= ~newbit;
    locp->changed = 1;

    if (hp->mmap) {
        hp->mmap[pos] = bits;
    } else {
        _write(locp, pos, &bits, 1);
    }
}

// _hxappend: append byte block to buffer.
void
_hxappend(HXBUF * bufp, char const *rp, COUNT leng)
{
    assert(leng > 0);
    assert(rp < bufp->data || rp >= bufp->data + bufp->used + leng);
    memcpy(&bufp->data[bufp->used], rp, leng);
    bufp->used += leng;
    STAIN(bufp);
}

int
_hxgetfreed(HXLOCAL * locp, HXBUF * bufp)
{
    if (!locp->freed)
        return 0;
    DEBUG3("getfreed pgno=%u", locp->freed);
    _hxsave(locp, bufp);
    _hxfresh(locp, bufp, locp->freed);
    locp->freed = 0;

    return 1;
}

// Insert (freed) pgno into the one-element "freed" cache.
//  If the cache is already occupied, flush it. The cache
//  entry is still marked 'used' in the on-disk bitmap.
void
_hxputfreed(HXLOCAL * locp, HXBUF * bufp)
{
    // "_hxflushfreed" overwrites bufp
    PAGENO  pg = bufp->pgno;

    DEBUG3("putfreed pgno=%u", pg);

    assert(!bufp->used && !IS_HEAD(bufp->pgno));
    assert(pg && pg != locp->freed);
    if (locp->freed)
        _hxflushfreed(locp, bufp);

    locp->freed = pg;
    SCRUB(bufp);
}

void
_hxflushfreed(HXLOCAL * locp, HXBUF * bufp)
{
    if (!locp->freed)
        return;
    DEBUG3("pgno=%u buf%d", locp->freed, BUFNUM(locp, bufp));
    _hxalloc(locp, locp->freed, 0);
    SCRUB(bufp);
    _hxfresh(locp, bufp, locp->freed);
    _hxsave(locp, bufp);
    locp->freed = 0;
}

// _hxfindfree: search bitmap pages for an empty overflow page.
int
_hxfindfree(HXLOCAL * locp, HXBUF * bufp)
{
    HXFILE *hp = locp->file;
    PAGENO  pgno = 0;
    int     dsize = DATASIZE(hp), ix = dsize;

    assert(!DIRTY(bufp));
    for (pgno = 0; pgno < locp->npages; pgno = NEXT_MAP(hp, pgno)) {
        _hxload(locp, bufp, 0);
        for (ix = bufp->used; ix < dsize && !(0xFF & ~bufp->data[ix]); ++ix);
    }

    if (pgno < locp->npages)
        DEBUG3("npages=%d map: pgno=%d used=%d data[%d]=%02X",
               locp->npages, pgno, bufp->used, ix,
               ix < dsize ? bufp->data[ix] : 0x666);
    if (ix < dsize)
        pgno +=
            (((ix - bufp->used) << 3) + ffs(~bufp->data[ix]) - 1) * HXPGRATE;
    if (pgno >= locp->npages)
        return 0;
    _hxalloc(locp, pgno, 1);
    _hxfresh(locp, bufp, pgno);
    return 1;
}

// _hxfresh: initialize a buffer as an empty page.
//  Marked dirty because I couldn't figure out how to make
//  the one NECESSARY case for DIRTY (growFile) work.
//  Also used in flushFreed ... maybe that's the problem.
void
_hxfresh(HXLOCAL const *locp, HXBUF * bufp, PAGENO pgno)
{
    HXFILE *hp = locp->file;

    DEBUG3("fresh pgno=%u buf%d", pgno, BUFNUM(locp, bufp));

    assert(!DIRTY(bufp));
    assert(pgno < locp->npages);
    assert(!
           (locp->buf[0].page && locp->buf[0].pgno == pgno &&
            DIRTY(&locp->buf[0])));
    assert(!
           (locp->buf[1].page && locp->buf[1].pgno == pgno &&
            DIRTY(&locp->buf[1])));
    assert(!
           (locp->buf[2].page && locp->buf[2].pgno == pgno &&
            DIRTY(&locp->buf[2])));

    bufp->next = bufp->used = bufp->recs = bufp->orig = bufp->flag = 0;
    bufp->pgno = pgno;
    bufp->hsize = bufp->hmask = 1;  //??? Is this right ??
    bufp->hind = (COUNT *) (bufp->data + DATASIZE(hp)) + 1;

    if (hp->mmap)
        MAP_BUF(hp, bufp);

    memset(bufp->page, 0, hp->pgsize);
    STAIN(bufp);                // TODO: why is this needed?
}

// _hxgrow: extend the file. Each "_hxgrow" creates one or more
//  empty pages. It is BARELY possible that a split will require
//  a new empty page in order to complete. In this case, _hxgrow
//  recurses.
void
_hxgrow(HXLOCAL * locp, HXBUF * retp, COUNT need, PAGENO * head)
{
    HXFILE *hp = locp->file;
    HXBUF  *newp = &locp->buf[0];
    HXBUF  *oldp = &locp->buf[1];
    HXBUF  *bufp = &locp->buf[2];

    _hxsave(locp, oldp);
    _hxsave(locp, newp);
    _hxsave(locp, bufp);

    while (1) {
        PAGENO  newpg = locp->npages++;

        if (IS_MAP(hp, newpg)) {
            // Page after a MAP is always a HEAD
            newpg = locp->npages++;
            _hxresize(locp, locp->npages);
            DEBUG3("grow map pgno=%d", newpg);
            _hxalloc(locp, newpg - 1, 1);
        } else {
            _hxresize(locp, locp->npages);
            if (!IS_HEAD(newpg)) {
                DEBUG3("grow ovfl pgno=%d", newpg);
                _hxalloc(locp, newpg, 1);
                _hxfresh(locp, retp, newpg);
                return;
            }
        }

        _hxpoint(locp);         // because npages has changed
        PAGENO oldpg = SPLIT_PAGE(locp);
        if (*head == oldpg) {
            DEBUG3("head=%u", *head);
            *head = need = 0;   // "need=0" blocks _hxshare
        }

        _hxlock(locp, oldpg, 1);
        _hxload(locp, oldp, oldpg);
        _hxfresh(locp, newp, newpg);
        // The split target WAS in BEYOND but the file is about
        // to grow. Need to explicitly bookkeep the target as
        // still properly locked, else _hxislocked will fail.
        if (hp->locked & LOCKED_BEYOND && !(hp->locked & LOCKED_BODY))
            _hxaddlock(hp, newpg);

        LINK(newp, oldp->next);
        _hxshift(locp, newpg, 0, oldp, newp, NULL);

        int     loops = HX_MAX_CHAIN;

        while (oldp->next) {
            if (!--loops)
                LEAVE(locp, HXERR_BAD_FILE);
            if (bufp->used) {
                _hxsave(locp, bufp);
            } else {
                SCRUB(bufp);
            }

            assert(oldp->next == newp->next || !newp->next);

            _hxload(locp, bufp, oldp->next);
            // _hxshift moves records from bufp to (oldp,newp).
            // Its return value indicates whether there are still
            // records in bufp for (both/either/neither) 
            // of oldp and newp. 
            switch (_hxshift(locp, oldpg, newpg, bufp, oldp, newp)) {
            case 0:
                LINK(oldp, bufp->next);
                LINK(newp, bufp->next);
                if (!bufp->used)
                    _hxputfreed(locp, bufp);
                break;

            case 1:
                LINK(newp, bufp->next);
                SWAP(oldp, bufp);
                break;

            case 2:
                LINK(oldp, bufp->next);
                SWAP(newp, bufp);
                break;

            case 3:
                SWAP(oldp, bufp);
                if (!oldp->next)
                    break;

                if (!_hxgetfreed(locp, bufp)) {
                    PAGENO  pgs[] = { oldp->pgno, newp->pgno };

                    // Worst case: _hxgrow needs a new
                    // ovfl page to complete this chain split.
                    _hxgrow(locp, bufp, 0, head);
                    _hxload(locp, oldp, pgs[0]);
                    _hxload(locp, newp, pgs[1]);
                }

                LINK(newp, bufp->pgno);
                SWAP(newp, bufp);
                LINK(newp, oldp->next);
                _hxshift(locp, newpg, 0, oldp, newp, NULL);
                break;
            }
        }

        // Necessary even if tail is not shared
        if (oldp->next && !FILE_HELD(hp)
            && !IS_HEAD(oldp->next))
            _hxunlock(locp, oldp->next, 1);

        _hxsave(locp, oldp);
        _hxsave(locp, newp);
        _hxsave(locp, bufp);

        if (_hxshare(locp, retp, need)
            || _hxgetfreed(locp, retp))
            return;
    }
    //NOTREACHED
}

// _hxindexify: construct in-page hash index.
//  hind points at the LAST of [hsize] COUNT fields.
//  If not supplied, hind inside bufp is used.
void
_hxindexify(HXLOCAL const *locp, HXBUF * bufp, COUNT * hind)
{
    int     hsize = HIND_SIZE(locp->file, bufp);

    if (!hind)
        hind = HIND_BASE(locp->file, bufp);
    memset(hind - hsize + 1, 0, hsize * sizeof(COUNT));

    // Pass #1: insert hashes with no collisions:

    int     nsaved = 0;
    struct hr { int     hpos, rpos; } save[bufp->recs];
    char const *recp, *endp;
    int     hmask = MASK(hsize);

    FOR_EACH_REC(recp, bufp, endp) {
        //int i = HIND_POS(bufp, RECHASH(recp));
        int     pos = recp - bufp->data + 1;
        int     i = RECHASH(recp) & hmask;

        if (i >= hsize)
            i &= hmask >> 1;
        if (hind[-i]) {
            save[nsaved++] = (struct hr) { i, pos};
        } else {
            hind[-i] = pos;
        }
    }

    // Pass #2: insert collisions:
    while (nsaved--) {
        int     i = save[nsaved].hpos;

        do
            i = (i ? i : hsize) - 1;
        while (hind[-i]);
        hind[-i] = save[nsaved].rpos;
    }
}

// _hxlink: update a "next" link without loading a HXBUF.
//  This should ONLY be used by hxfix.
void
_hxlink(HXLOCAL * locp, PAGENO pg, PAGENO nextpg)
{
    PAGENO  next;
    HXFILE *hp = locp->file;
    off_t   pos = (off_t) pg * hp->pgsize;

    DEBUG2("pgno=%u next=%d", pg, nextpg);

    assert(!IS_MAP(hp, pg));
    assert(!nextpg || !IS_MAP(hp, nextpg));
    assert(!IS_HEAD(nextpg));
    assert(nextpg != pg);

    locp->changed = 1;

    STLG(nextpg, &next);
    _write(locp, pos, &next, sizeof next);
}

// _hxmove: change a buffer's pgno. For MMAP, this requires
//  copying BYTEs; for non-MMAP, this just changes .pgno.
void
_hxmove(HXLOCAL const *locp, HXBUF * bufp, PAGENO pgno)
{
    (void)locp;
    //HXFILE    *hp = locp->file;

    assert(!DIRTY(bufp));
    //Currently, only hxshape calls _hxmove,
    // and hxshape does not allow an mmap'ed file.
    assert(!locp->file->mmap);
#   if 0
    if (hp->mmap)
        memcpy(&hp->mmap[(off_t) pgno * hp->pgsize], bufp->page, hp->pgsize);
#   endif
    STAIN(bufp);
    bufp->pgno = pgno;
}

// _hxremove: Delete byte-block (record) from buffer.
void
_hxremove(HXBUF * bufp, COUNT pos, COUNT leng)
{
    char   *delp = bufp->data + pos;

    assert(leng > 0 && leng <= bufp->used);
    assert(pos <= bufp->used);

    bufp->used -= leng;
    memmove(delp, delp + leng, bufp->used - pos);
    STAIN(bufp);
}

// _hxresize: set file size (shrink or extend).
void
_hxresize(HXLOCAL * locp, PAGENO npgs)
{
    HXFILE *hp = locp->file;

    if (ftruncate(hp->fileno, (off_t) npgs * hp->pgsize))
        LEAVE(locp, HXERR_FTRUNCATE);

    locp->npages = npgs;
    locp->dpages = _hxf2d(locp->npages);
    locp->mask = MASK(locp->dpages);
    _hxpoint(locp);

    if (hp->mmap)
        _hxremap(locp);
}

// _hxsave: write a (dirty) page back to file. If page is
//      suitable for sharing, record its (pgno,used).
void
_hxsave(HXLOCAL * locp, HXBUF * bufp)
{
    if (!DIRTY(bufp))
        return;

    HXFILE *hp = locp->file;
    COUNT   pgsize = hp->pgsize;

    bufp->hsize = HIND_SIZE(hp, bufp);
    bufp->hmask = MASK(bufp->hsize);
    if (hxdebug > 2) {
        char    headstr[9999] = "<what>";

        DEBUG3
            ("save pgno=%u next=%u used=%u recs=%d hsize=%d orig=%u tail=%u heads=%s",
             bufp->pgno, bufp->next, bufp->used, bufp->recs, bufp->hsize,
             bufp->orig, hp->tail.pgno, _hxheads(locp, bufp, headstr));
    }
    assert(!bufp->pgno || !bufp->next || bufp->used);
    assert(!bufp->pgno || bufp->next != bufp->pgno);
    assert(FITS(hp, bufp, 0, 0));   // Check that (used,recs) are not out of bounds.
    assert(!IS_MAP(hp, bufp->pgno) || bufp->data[bufp->used] & 1);
    // Ensure that the same page is modified, in two buffers!
    assert(!
           (locp->buf[0].pgno == bufp->pgno && bufp != &locp->buf[0] &&
            DIRTY(&locp->buf[0])));
    assert(!
           (locp->buf[1].pgno == bufp->pgno && bufp != &locp->buf[1] &&
            DIRTY(&locp->buf[1])));
    assert(!
           (locp->buf[2].pgno == bufp->pgno && bufp != &locp->buf[2] &&
            DIRTY(&locp->buf[2])));

    locp->changed = 1;

    STLG(bufp->next, &bufp->page->next);
    STSH(bufp->used, &bufp->page->used);
    STSH(bufp->recs, &bufp->page->recs);

    if (!IS_MAP(hp, bufp->pgno))
        _hxindexify(locp, bufp, NULL);

#if 0
    int     rc;
    if (!(hp->mode & HX_REPAIR) &&
        HXOKAY != (rc = _hxcheckbuf(locp, bufp))) {;
        hxdebug = 1;
        fprintf(stderr, "_hxcheckbuf failed: %d %s\n", rc, hxcheck_namev[rc]);
        _hxprbuf(locp, bufp, stderr);
        _hxprloc(locp, stderr);
        abort();
    }
#endif
    if (!hp->mmap)
        _write(locp, (off_t) bufp->pgno * pgsize, bufp->page, pgsize);

    SCRUB(bufp);

    if (IS_HEAD(bufp->pgno) || IS_MAP(hp, bufp->pgno))
        return;

    if (hp->tail.pgno == bufp->pgno) {
        if (bufp->next)
            hp->tail.used = DATASIZE(hp);   // mark tail as unsharable.
        else
            hp->tail = *bufp;   // update (used,recs)
    } else if (!bufp->next && hp->tail.used >= bufp->used)
        hp->tail = *bufp;       // update (pgno,used,recs)

    assert(hp->tail.next == 0);
}

// _hxshare: return 1, and set up buffer, if the last tail
// page SAVE'd has (need) bytes free. "tail.pgno" is a HINT:
// the page may have changed since this process last touched it.

int
_hxshare(HXLOCAL * locp, HXBUF * bufp, COUNT need)
{
    HXFILE *hp = locp->file;

    if (!need || !FITS(hp, &hp->tail, need, 1))
        return 0;

    _hxload(locp, bufp, hp->tail.pgno);
    DEBUG3("pgno=%d used=%u next=%d need=%u", bufp->pgno, bufp->used,
           bufp->next, need);
    if (bufp->next || !FITS(hp, bufp, need, 1))
        return 0;
    if (!bufp->used)
        _hxalloc(locp, bufp->pgno, 1);

    return 1;
}

// _hxshift: move records from srcp to lowerp and upperp,
//  depending on where each record's hash maps it, until the
//  target(s) are full. Records that do not belong in either,
//  or do not fit in either, are left in (srcp).
// RETURNS bitmask where bits [0,1] mean records left
// not moved from srcp to [lowerp,upperp].
// If called with lowerp == upperp, return 3 rather than 1.
int
_hxshift(HXLOCAL const *locp, PAGENO lo, PAGENO hi,
         HXBUF * srcp, HXBUF * lowerp, HXBUF * upperp)
{
    COUNT   size;
    HXFILE *hp = locp->file;
    HXBUF  *dstv[] = { srcp, lowerp, upperp };
    char   *recp = srcp->data;
    char   *endp = recp + srcp->used;
    int     filled = 0;

    assert((lo != hi) || (lowerp == upperp));
    srcp->used = srcp->recs = 0;    // prep for _hxappend(srcp,...)

    // NOT a FOR_EACH_REC loop here, since srcp->used
    //  has been zapped (to be both an input and an output).
    for (; recp < endp; recp += size) {
        PAGENO  test = _hxhead(locp, RECHASH(recp));
        int     whither = test == hi ? 2 : test == lo ? 1 : 0;
        HXBUF  *dstp = dstv[whither];

        size = RECSIZE(recp);
        if (!FITS(hp, dstp, size, 1)) {
            dstp = srcp;
            filled |= lowerp == upperp ? 3 : whither;
            if (filled == 3 && (unsigned)size * HXPGRATE
                < (unsigned)DATASIZE(hp)) {
                size = endp - recp;
            }
        }

        if (recp != dstp->data + dstp->used) {
            memmove(&dstp->data[dstp->used], recp, size);
            STAIN(dstp);
        }
        dstp->used += size;
        dstp->recs++;
    }

    if (SHRUNK(srcp))
        STAIN(srcp);

    return filled;
}

int
_hxtemp(HXLOCAL * locp, char *vbuf, int vbufsize)
{
    FILE   *fp = tmpfile();

    if (!fp)
        LEAVE(locp, HXERR_FOPEN);

    setvbuf(fp, vbuf, vbuf ? _IOFBF : _IONBF, vbufsize);
    return fileno(locp->fp[locp->nfps++] = fp);
}

//--------------|---------------------------------------------
static void
_write(HXLOCAL * locp, off_t pos, void *buf, int len)
{
    if (hxcrash > 0 && !--hxcrash)
        exit(9);
    if (0 > lseek(locp->file->fileno, pos, SEEK_SET))
        LEAVE(locp, HXERR_LSEEK);

    if (0 > write(locp->file->fileno, buf, len))
        LEAVE(locp, HXERR_WRITE);
}

//EOF
