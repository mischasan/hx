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
// hx: utilities for implementing HX file access method.

#include "_hx.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include "util.h"               // fls

int     hxcrash;
int     hxdebug;
char const *hxproc = "";
double  hxtime;
int     hxversion = HXVERSION;

int
hx_diff(HXFILE const *hp, char const *reca, char const *recb)
{
    return hp->diff(reca, recb, hp->udata, hp->uleng);
}

HXHASH
hx_hash(HXFILE const *hp, char const *recp)
{
    return hp->hash(recp, hp->udata, hp->uleng);
}

int
hx_load(HXFILE const *hp, char *recp, int recsize, char const *buf)
{
    if (!hp->load)
        return HXERR_BAD_REQUEST;

    int     rc = hp->load(recp, recsize, buf, hp->udata, hp->uleng);

    return rc > 0 && rc <= hxmaxrec(hp) ? rc : HXERR_BAD_RECORD;
}

int
hx_save(HXFILE const *hp, char const *recp, int reclen,
        char *buf, int bufsize)
{
    return !hp->save ? HXERR_BAD_REQUEST
        : hp->save(recp, reclen, buf, bufsize, hp->udata, hp->uleng);
}

int
hx_test(HXFILE const *hp, char const *recp, int reclen)
{
    return hp->test ? hp->test(recp, reclen, hp->udata, hp->uleng)
        : HXERR_BAD_REQUEST;
}

int
hxfileno(HXFILE const *hp)
{
    return hp ? hp->fileno : -1;
}

int
hxinfo(HXFILE const *hp, char *udata, int usize)
{
    if (!hp)
        return HXERR_BAD_REQUEST;
    memcpy(udata, hp->udata, IMIN(hp->uleng, usize));
    return hp->uleng;
}

int
hxmaxrec(HXFILE const *hp)
{
    return hp ? (int)(DATASIZE(hp) - sizeof(HXREC) - MIN_INDEX_BYTES(1))
        : HXERR_BAD_REQUEST;
}

//--------------|-------|-------------------------------------
// _hxenter: common prologue validations and processing
//        for API routines. Check args, initialize HXLOCAL,
//  allocate buffers, handle generic lock work.
void
_hxenter(HXLOCAL * locp, HXFILE * hp, char const *recp, int nbufs)
{
    assert(nbufs <= MAXHXBUFS);

    // The following must be set before any LEAVE:
    locp->file = hp;

    if (!hp || !hp->diff || !hp->hash)
        LEAVE(locp, HXERR_BAD_REQUEST);

    locp->mode = F_WRLCK;       // the common case
    errno = 0;
    if (recp)
        locp->hash = hx_hash(hp, recp);

    // THIS SHOULD NOT BE HERE!
    if (!IS_MMAP(hp) && nbufs) {
        char   *mem = calloc(nbufs, hp->pgsize);
        int     i;

        for (i = 0; i < nbufs; ++i)
            locp->buf[i].page = (HXPAGE *) & mem[i * hp->pgsize];
    }
}

// _hxfind: search buffer for match against given key.
//  Returns (-1), or offset in data[] of HXREC matching (rp).
// *hindp is the hash table position, for incremental hind changes.
int
_hxfind(HXLOCAL * locp, HXBUF const *bufp, HXHASH rechash,
        char const *recdata, int *hindp)
{
    HXFILE const *hp = locp->file;
    COUNT const *hind = HIND_BASE_C(hp, bufp);
    int     hsize = HIND_SIZE(hp, bufp);
    unsigned mask = MASK(hsize);
    int     i = rechash & mask;

    if (i >= hsize)
        i &= mask >> 1;

    for (; hind[-i]; i = (i ? i : hsize) - 1) {
        char const *recp = bufp->data + hind[-i] - 1;

        if (rechash == RECHASH(recp) && !hx_diff(hp, recdata, RECDATA(recp)))
            break;
    }
    if (hindp)
        *hindp = i;
    return hind[-i] - 1;
}

// _hxhead: calculate pgno of head-of-chain for a given
//            record hash.
// This first reverses the hash, since the low bits 
// of the hash are now used for in-page indexing.
PAGENO
_hxhead(HXLOCAL const *locp, HXHASH hash)
{
    PAGENO  pg = REV_HASH(hash) & locp->mask;

    return _hxd2f(pg < locp->dpages ? pg : pg & (locp->mask >> 1));
}

// _hxindexed: test that hind[] is correct.
//  - every record is indexed
//  - the rest of hind[] is zero
int
_hxindexed(HXLOCAL * locp, HXBUF const *bufp)
{
    COUNT const *hind = HIND_BASE_C(locp->file, bufp);
    int     hsize = HIND_SIZE(locp->file, bufp);
    COUNT   test[hsize], *tend = test + hsize - 1;

    memcpy(test, hind - hsize + 1, hsize * sizeof(COUNT));

    // Ensure that there are at least hsize/4 zeroes in hind[]:
    //  _hxfind relies on there being at least 1 zero!

    int     i = 0, hpos = bufp->recs / 4;

    while (i < hsize && hpos > 0)
        hpos -= !test[i++];
    if (i == hsize)
        return 0;

    char const *recp, *endp;

    FOR_EACH_REC(recp, bufp, endp) {
        if (0 > _hxfind(locp, bufp, RECHASH(recp), RECDATA(recp), &hpos))
            return 0;
        tend[-hpos] = 0;
    }

    for (i = 0; i < hsize; ++i)
        if (tend[-i])
            return 0;

    return 1;
}

// _hxleave: epilogue clean-up.
//  POSIX says free(NULL) is okay ...
HXRET
_hxleave(HXLOCAL * locp)
{
    HXFILE *hp = locp->file;
    int     i, save_errno = errno;

    // Handle _hxenter rejecting !hp as a pass-through
    if (!hp)
        return HXERR_BAD_REQUEST;

    if (!hp->hold && locp->mylock)
        _hxunlock(locp, 0, 0);

    // No point in dumping core if you can't flush anyway.
    if (locp->ret >= 0) {
        assert(!locp->freed);
        assert(!locp->buf[0].page || !DIRTY(&locp->buf[0]));
        assert(!locp->buf[1].page || !DIRTY(&locp->buf[1]));
        assert(!locp->buf[2].page || !DIRTY(&locp->buf[2]));
    }

    if (!IS_MMAP(hp))
        free(locp->buf[0].page);
    else if (hp->mode & HX_MPROTECT)
        mprotect(hp->mmap, hp->mlen, PROT_NONE);

    if (locp->changed && (hp->mode & HX_FSYNC)
        && locp->ret >= HXOKAY && fsync(hp->fileno))
        locp->ret = HXERR_FSYNC;

    free(locp->vnext);
    free(locp->vprev);
    free(locp->vrefs);
    free(locp->visit);
    free(locp->vtail);
    free(locp->recv);
    free(locp->membase);

    if (locp->tmpmap)
        munmap(locp->tmpmap, locp->mapsize);

    for (i = locp->nfps; --i >= 0;)
        fclose(locp->fp[i]);

    errno = save_errno;
    return locp->ret;
}

// _hxload: read page into a buffer, setting dependent
//            buffer fields.
void
_hxload(HXLOCAL * locp, HXBUF * bufp, PAGENO pgno)
{
    HXFILE *hp = locp->file;
    off_t   pos = (off_t) pgno * hp->pgsize;

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

    if (!IS_HEAD(pgno))
        _hxlock(locp, pgno, 1);
    if (!hp->hold && !_hxislocked(locp, pgno)) {
        DEBUG("no lock guarding pgno=%d", pgno);
        assert(!"page is not locked");
    }

    if (IS_MMAP(hp))
        bufp->page = (HXPAGE *) & hp->mmap[pos];
    else
        _hxread(locp, pos, bufp->page, hp->pgsize);

    bufp->pgno = pgno;
    bufp->next = LDUL(&bufp->page->next);
    bufp->used = LDUS(&bufp->page->used);
    bufp->recs = LDUS(&bufp->page->recs);
    bufp->orig = bufp->used;
    bufp->hind = (COUNT *) (bufp->data + DATASIZE(hp)) - 1;
    bufp->hsize = HIND_SIZE(hp, bufp);
    bufp->hmask = MASK(bufp->hsize);
    SCRUB(bufp);

    if (!IS_MAP(hp, pgno) && hxdebug > 2) {
        char    heads[9999];

        DEBUG3("load pgno=%u next=%u used=%u recs=%u hsize=%d heads=%s",
               pgno, bufp->next, bufp->used, bufp->recs, bufp->hsize,
               _hxheads(locp, bufp, heads));
        //assert(_hxcheckbuf(locp, bufp) == HXOKAY);
    } else {
        DEBUG3("load pgno=%u", pgno);
    }

    if (!(hp->mode & HX_RECOVER) && ((bufp->used > DATASIZE(hp))
                                     || (pgno && bufp->next && !bufp->used)
                                     || (pgno && IS_HEAD(bufp->next))
                                     || (IS_MAP(hp, pgno) &&
                                         !(bufp->data[bufp->used] & 1))
        ))
        LEAVE(locp, HXERR_BAD_FILE);
}

// _hxmap: translate (ovfl) pgno to a bitmap pos(pgno,bitpos).
//  "map1" is the pgno of the first map page (after page 0).
PAGENO
_hxmap(HXFILE const *hp, PAGENO pgno, int *bitpos)
{
    PAGENO  mpg = 0, ppm;

    assert(!IS_HEAD(pgno));

    if (pgno < hp->map1) {

        *bitpos = pgno / HXPGRATE + 8 * hp->uleng;

    } else {

        ppm = 8 * HXPGRATE * DATASIZE(hp);
        mpg = pgno - (pgno - hp->map1) % ppm;
        *bitpos = (pgno - mpg) / HXPGRATE % ppm;
        DEBUG3("pgno=%u: %u %u", pgno, mpg, *bitpos);
    }
    DEBUG3("pgno=%u maps to mpg=%u off=%u bit=%u",
           pgno, mpg, *bitpos / 8, *bitpos & 7);
    return mpg;
}

// _hxpginfo: read the {next,used} page header (only).
//  Currently only of use to "hxshare", which forbids MMAP.
PGINFO
_hxpginfo(HXLOCAL * locp, PAGENO pg)
{
    PGINFO  info;

    assert(!locp->file->mmap);
    _hxread(locp, (off_t) pg * locp->file->pgsize, &info, sizeof info);
    return (PGINFO) {
    LDUL(&info.pgno), LDUS(&info.used), LDUS(&info.recs)};
}

// _hxpoint: update loc.head from (npages,hash).
//  
void
_hxpoint(HXLOCAL * locp)
{
    HXFILE *hp = locp->file;
    PAGENO  head = _hxhead(locp, locp->hash);

    // This is the first place where we can calc the curr key head.
    // Cancel a prior hxhold() for a different key
    if (hp->hold && hp->hold != head && !FILE_HELD(hp)) {
        _hxunlock(locp, hp->hold, 1);
        hp->hold = 0, hp->lockpart = NONE_LOCK;
    }

    locp->head = head;
}

void
_hxread(HXLOCAL * locp, off_t pos, void *buf, int size)
{
    int     ret;

    assert(pos < (off_t) locp->npages * locp->file->pgsize);
    assert(pos + size <= (off_t) locp->npages * locp->file->pgsize);
    if (pos != lseek(locp->file->fileno, pos, SEEK_SET))
        LEAVE(locp, HXERR_LSEEK);
    ret = read(locp->file->fileno, buf, size);
    assert(ret == size);
    if (size != ret)
        LEAVE(locp, HXERR_READ);
}

void
_hxremap(HXLOCAL * locp)
{
    HXFILE *hp = locp->file;
    off_t   mlen = (off_t) hp->pgsize * locp->npages;
    int     prot = hp->mode & HX_UPDATE ? PROT_WRITE : 0;

    // At hxopen, hp->mlen is 0
    if (hp->mlen == mlen) {

        if (hp->mode & HX_MPROTECT
            && mprotect(hp->mmap, hp->mlen, prot | PROT_READ))
            LEAVE(locp, HXERR_MMAP);

    } else {
        char   *mp;

        if (hp->mmap) {
#           ifdef __USE_GNU
            mp = mremap(hp->mmap, hp->mlen, mlen, MREMAP_MAYMOVE);
#           else
            if (munmap(hp->mmap, hp->mlen))
                LEAVE(locp, HXERR_MMAP);
            mp = mmap(hp->mmap, mlen, prot | PROT_READ,
                      MAP_SHARED | MAP_NOCORE, hp->fileno, 0);
#endif
        } else {
            mp = mmap( /*addr */ NULL, mlen, PROT_READ | prot,
                      MAP_SHARED | MAP_NOCORE, hp->fileno, 0);
        }

        DEBUG("mmap %d %s%d -> %d%s", hp->mlen, mlen < hp->mlen ? "" : "+", mlen - hp->mlen, mlen, !hp->mlen || hp->mmap == mp ? "" : " MOVED");
        if (!mp || mp == MAP_FAILED)
            LEAVE(locp, HXERR_MMAP);

        hp->mlen = mlen;
        hp->mmap = mp;
        HXBUF  *bufp = &locp->buf[MAXHXBUFS];

        while (--bufp >= locp->buf)
            if (bufp->page)
                MAP_BUF(hp, bufp);
    }
}

void
_hxsize(HXLOCAL * locp)
{
    HXFILE *hp = locp->file;
    off_t size = lseek(hp->fileno, 0, SEEK_END);

    if (size % hp->pgsize || (size /= hp->pgsize) < 2)
        LEAVE(locp, HXERR_BAD_FILE);
    PAGENO npages = locp->npages, split = SPLIT_PAGE(locp);

    locp->npages = size;
    locp->dpages = _hxf2d(locp->npages);
    locp->mask = MASK(locp->dpages);

    if (npages && npages != locp->npages)
        DEBUG2("npages changes: %d<%d> to %d<%d>",
               npages, split, locp->npages, SPLIT_PAGE(locp));
}

//EOF
