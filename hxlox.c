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
// hxlox: utilities for implementing HX file/page locking.
//
// For hxput: after _hxlockset(HIGH), locp->head must
//  stay locked until hxput exits. Either splits is at least
//  partly greater than head (and is also locked on HIGH),
//  or splits cannot move higher than head, because head is locked.
//  The file may grow due to splitting, as long as splits stays
//  completely above or below head. WAIT increasing file size
//  may eventually make splits wrap around to (1,2,3) again.
//  As long as splits does not cross head, no file size change
//  can affect the hash->head calculation (_hxpoint)

#include <assert.h>
#include <errno.h>
#include <fcntl.h>        // F_??LCK; NULL if no stdio.h
#include <stdarg.h>

#include "_hx.h"

static PAGENO *    findlock(HXFILE*, PAGENO);
static void        _lock(HXLOCAL*, off_t, short, off_t);
static char const* lockstr(HXFILE const*, char *buf);

static char const*  modename[] = { "R", "W", "U" };
//static char const*  partname[] = { "NONE", "HEAD", "HIGH", "BOTH" };
//--------------|-------|-------------------------------------
void
_hxaddlock(HXFILE *hp, PAGENO pg)
{
    assert(pg);
    PAGENO *pp = findlock(hp, pg);
    assert(!*pp);
    *pp = pg;
}

// NOTE: When npages < HXPGRATE+2, hxlockset will
//  have locked page 1, even if it hasn't locked (2,3,5).
int
_hxislocked(HXLOCAL const*locp, PAGENO pg)
{
    HXFILE      *hp = locp->file;
    return !pg                 ? hp->locked & LOCKED_ROOT
         :  pg >= locp->npages ? hp->locked & LOCKED_BEYOND
                               : hp->locked & LOCKED_BODY
                                 || *findlock(hp, pg);
}

// _hxlock: lock file or page(s). No special EINTR handling.
//  count == 0 means lock from pgno to EOF, or lock all future
//  pages beyond EOF if pgno == npages.
void
_hxlock(HXLOCAL *locp, PAGENO pgno, COUNT count)
{
    HXFILE      *hp    = locp->file;
    off_t       pgsize = hp->pgsize;
    char        str[99];
    DEBUG3("lock start=%u count=%d npages=%d < %s",
            pgno, count, locp->npages, lockstr(hp,str));

    assert(hp->mode & HX_UPDATE || locp->mode == F_RDLCK);
    assert(     count == 1 // ROOT and OVFL and some HEAD pages
            || (count == 0 && pgno == 0) //FILE or BODY lock
            || (count == 0 && pgno == locp->npages) //BEYOND
            || (count < HXPGRATE && IS_HEAD(pgno)));

        // Attempting to RE-lock a page can EDEADLK !?
    if (!pgno && !count) {
        if (hp->locked & LOCKED_ROOT) pgno = 1;
        if (hp->locked & LOCKED_BODY) count = 1;
        if (pgno && count) return;
    }

    if (pgno == 0 && count == 1 && hp->locked & LOCKED_ROOT) return;
    if (pgno == 1 && count == 0 && hp->locked & LOCKED_BODY) return;
    if (pgno >= locp->npages  && hp->locked & LOCKED_BEYOND) return;
    if (pgno && count           && hp->locked & LOCKED_BODY) return;

        // Skip pages already locked
    if (count) {
        while (count && _hxislocked(locp, pgno)) ++pgno, --count;
        if (!count) return;
    }

    // Warn about odd lock orders: so far this is always safe
    // The tricky one is when extending the file requires a
    //  page number to be added to lockv, even though no
    //  _lock() is done (not needed). This satisfies the check
    // in (_hxsave?_hxload?_hxfresh) that any page that is
    //  (saved,loaded,cleared) has a lock covering it.

    if (hxdebug && count && pgno < locp->npages
            && IS_HEAD(pgno)) {
        PAGENO  *pp = hp->lockv;
        while (*pp && (!IS_HEAD(*pp) || pgno <= *pp)) ++pp;
        if (*pp) DEBUG("pgno=%u count=%d after %u", pgno, count, *pp);
    }

    locp->mylock = 1;
    _lock(locp, pgno * pgsize, locp->mode, count * pgsize);

    if (!count)
        hp->locked |= pgno < 2 ? LOCKED_BODY : LOCKED_BEYOND;
    if (!pgno)
        hp->locked |= LOCKED_ROOT;
    else
        for (; count; --count) _hxaddlock(hp, pgno++);
    DEBUG3("lock > %s", lockstr(hp,str));
}

// _hxlockset: lock page(s) for a particular key. Used by
//  hxget/hxhold/hxput/hxdel. The problem: between the time that
//  _hxpoint computes the head page using the file size, and the
//  time the lock is granted, the file size may change ... so
//  the lock must be reacquired. Note that hxhold must
//  (pessimistically) anticipate a hxput.
//
//  "part" can have one of these *_LOCK values:
//  HEAD - just lock head page. Called by hxget and hxdel.
//  HIGH - lock head page, plus all split pages if any split
//          is > head. Called by hxhold and (initially) hxput.
//  BOTH - lock split pages, which are all > head. Called by hxput
//          when it needs more space for the record to update,
//          but _hxlockset(*,HIGH_LOCK) did not lock the split.
// hxlockset(*,HIGH_LOCK) can set lockpart to HIGH_LOCK or BOTH_LOCK.
// hxlockset must anticipate MAP pages, in which case "split"
//  must lock an extra 3 pages (see _calcsplits).
// This looks too messy. Also, _hxmap must always follow _hxlockset
// After the SPLIT is locked

void
_hxlockset(HXLOCAL *locp, LOCKPART part)
{
    HXFILE *hp   = locp->file;

    if (hp->lockpart >= part || FILE_HELD(hp)) {
        _hxsize(locp);
        _hxpoint(locp);
        return;
    }

    if ((int)locp->npages < HXPGRATE * 2) {
        _hxlock(locp, 0, 0);
        hp->lockpart = BOTH_LOCK;
        _hxsize(locp);
        _hxpoint(locp);
        return;
    }

    hp->lockpart = part;

    PAGENO pgv[2*HXPGRATE] = {}, *pp, oldsize = 0;

    while (1) {
        _hxsize(locp);
        _hxpoint(locp); // calculate locp->head
        if (oldsize == locp->npages) break;
        oldsize = locp->npages;
        for (pp = pgv; *pp; ++pp) _hxunlock(locp, *pp, 1);

        pp = pgv;
        if (part != BOTH_LOCK) *pp++ = locp->head;
        *pp = 0;
        if (part != HEAD_LOCK) _hxsplits(hp, pgv, locp->npages);

        // If head is higher than any split, just lock head:
        if (part == HIGH_LOCK && pgv[0] == locp->head)
            pgv[1] = 0;

        // Minimize lock syscalls by locking sequential pages
        //  with a single call.
        int ct;
        for (pp = pgv; *pp; pp += ct) {
            for (ct = 1; pp[ct] && pp[ct] + ct == *pp; ++ct);
            _hxlock(locp, *pp - ct + 1, ct);
        }
    }

    // If we only locked the (head), the file's size may change (!)
    //  while we are working with it, but the chain starting at (head)
    //  can never refer to a page beyond locp->npages.
}

void
_hxunlock(HXLOCAL *locp, PAGENO start, PAGENO count)
{
    HXFILE      *hp = locp->file;
    int         pgsize = hp->pgsize;
    char        str[99];
    DEBUG3("unlock start=%u count=%d npages=%d < %s",
            start, count, locp->npages, lockstr(hp, str));

    assert(    (start == 0 && count == 0)
            || (start == 1) // see _hxlockset
            || (start == locp->npages && count == 0)
            || (!IS_HEAD(start) && count == 1));

    if (!count)
        hp->lockpart = NONE_LOCK;
    if (!count || !FILE_HELD(hp))
        _lock(locp, start * pgsize, F_UNLCK, count * pgsize);
    if (!start)
        hp->locked &= ~LOCKED_ROOT;
    if (!count)
        hp->locked &= ~LOCKED_BODY & ~LOCKED_BEYOND;
    if (!count)
        count = locp->npages;

    // Delete locked pages from lockv:
    PAGENO  *p = hp->lockv, *pp = p;
    while (*pp) ++pp;
    while (p < pp) {
        if ((unsigned)(*p - start) >= count) ++p;
        else *p = *--pp, *pp = 0;
    }
    DEBUG3("unlock > %s", lockstr(hp, str));
}

// _hxsplits is not static so that split_t can test it in detail.
void
_hxsplits(HXFILE *hp, PAGENO *pgv, PAGENO newpg)
{
    for (; IS_HEAD(newpg) || IS_MAP(hp, newpg); ++newpg) {
        if (IS_HEAD(newpg)) {
            // Compute split page:
            PAGENO *pp = pgv, pg = _hxf2d(newpg);
            pg = _hxd2f(pg - ((_hxmask(pg + 1) + 1) >> 1));
            // Insert split page into pgv[], retaining
            //  descending order and terminal (0):
            while (*pp > pg) ++pp;
            if (*pp < pg) {
                do SWAP(pg, *pp); while (*pp++);
                *pp = 0;
            }
        }
    }
}

// Return ptr to HXLOCAL lockv entry containing PAGENO,
//  else return pointer to next free entry (contains 0).
static PAGENO *
findlock(HXFILE *hp, PAGENO pg)
{
    PAGENO *pp = hp->lockv;
    for (; *pp && pg != *pp; ++pp);
    return  pp;
}

static void
_lock(HXLOCAL *locp, off_t pos, short mode, off_t len)
{
    HXFILE      *hp = locp->file;
    struct flock    what;
    what.l_type   = mode;
    what.l_start  = pos;
    what.l_len    = len;
    what.l_whence = SEEK_SET;
    what.l_pid    = 0;

    do errno = 0;
    while(fcntl(hp->fileno, F_SETLKW, &what) && errno == EINTR);

    if (errno && errno != EINTR) {
        int     save = errno;
        char    str[99];
        what.l_whence = SEEK_SET;
        fcntl(hp->fileno, F_GETLK, &what);
        DEBUG("LOCK ERROR: %lu(%lu) mode=%s errno %d='%s' locks %s blocked by "
                "%d(%d) pid:%d",
                (PAGENO)(pos / hp->pgsize),
                (PAGENO)(len / hp->pgsize),
                (unsigned)mode <= F_UNLCK ? modename[mode] : "???",
                save, strerror(save), lockstr(hp, str),
                (PAGENO)((int)what.l_start / hp->pgsize),
                (COUNT)(what.l_len / hp->pgsize), what.l_pid);
        errno = save;
        assert(errno != EDEADLK);
        LEAVE(locp, HXERR_LOCK);
    }
}

// lockstr: format locked-state as a string, for diagnostics.
static char const*
lockstr(HXFILE const*hp, char *buf)
{
    buf[0] = hp->locked & LOCKED_ROOT   ? 'R' : '-';
    buf[1] = hp->locked & LOCKED_BODY   ? 'B' : '-';
    buf[2] = hp->locked & LOCKED_BEYOND ? 'X' : '-';

    char *cp = buf+3;
    PAGENO const *pp = hp->lockv;
    while (*pp && cp < buf+90) cp += sprintf(cp, " %u", *pp++);
    buf[3] = '(';
    strcpy(cp + (cp == buf+3), ")");
    return buf;
}

//EOF
