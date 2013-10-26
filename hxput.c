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
// SYNOPSIS
//  int hxput(HXFILE *hp, char *rec, int leng)
//
// DESCRIPTION
//  "hxput" writes a record into a file. If there was a prior
//  record with a matching key, that prior record is first
//  deleted. So, if "leng" is (0), hxput amounts to a delete.
//
// RETURNS
//  <0  an error
//   0  record inserted
//  >0  length of record replaced/deleted
//
// IMPLEMENTATION
//  "hxput" searches the chain of pages in which the given
//  record's key must be found. When it encounters a previous
//  record with matching key, it deletes that record.
//  When it finds enough space in a page for the new record,
//  "hxput" inserts it. As "hxput" reads the chain of pages,
//  it attempts to compact (shift) records towards the start
//  of the chain. When the new record has been inserted,
//  and the old record has been removed, "hxput" traverses
//  the rest of the chain, until it reaches a point where no
//  further compaction can be done. If an overflow page is
//  completely emptied, it is booked into the free map, and
//  compaction continues with the next page.
//
//  Overflow pages are controlled by a bitmap stored in
//  overflow pages of the file. The first bitmap is in page 0,
//  following the user data passed to "hxcreate". Splitting a
//  page may release and re-allocate overflow pages frequently.
//  For this reason, one freed overflow pgno is cached for the
//  duration of the "hxput" call. Such a pgno is always marked
//  "allocated" in the bitmap.
//
// If there is no space in a chain, space must be found:
//  - if the chain has no overflow pages yet, check if a
//  recently-saved page can be shared ("_hxshare").
//  A "tail" overflow page (next==0) can be shared.
//  - check whether a page has been freed ("_hxgetfreed") by
//      this hxput in traversing the chain and compacting pages.
//  - check the bitmap of allocated overflow pages for an
//  unallocated page ("_hxfindfree")
//  - grow the file, by splitting an existing chain.
//  Splitting a chain may result in zero or more overflow
//  pages being released. The chain being split may be the
//  chain into which the new record was to be inserted.
//  In the latter case, the search for an insertion point
//  must restart at the head of chain: either at the
//  original head, or at a new head just added to the file.
//
// hxnext and hxput:
//  It is possible to update a file in the middle of a hxnext
//  scan, though with limitations:
//  - deleting the current record is always okay
//  - updating the current record without increasing its size
//  is always okay
//  - increasing a record's size will USUALLY work, but may
//  get a HXERR_BAD_REQUEST response if the hxput would
//  cause records to be missed by hxnext.

#include <assert.h>

#include "_hx.h"

//static void reindex(HXLOCAL*, HXBUF*);
static void sync_save(HXLOCAL *, HXBUF *);

//--------------|---------------------------------------------
HXRET
hxput(HXFILE * hp, char const *recp, int leng)
{
    HXLOCAL loc, *locp = &loc;

    if (!hp || leng < 0 || !recp || leng > hxmaxrec(hp)
        || !(hp->mode & HX_UPDATE) || !hp->test)
        return HXERR_BAD_REQUEST;

    if (leng && !hx_test(hp, recp, leng))
        return HXERR_BAD_RECORD;

    if (SCANNING(hp) && hx_diff(hp, recp, RECDATA(_hxcurrec(hp))))
        return HXERR_BAD_REQUEST;

    ENTER(locp, hp, recp, 3);
    _hxlockset(locp, leng ? HIGH_LOCK : HEAD_LOCK);
    if (IS_MMAP(hp))
        _hxremap(locp);

    int     may_find = 1, loops = HX_MAX_CHAIN;
    int     newsize = leng ? leng + sizeof(HXREC) : 0;
    HXBUF  *currp = &locp->buf[0], *prevp = &locp->buf[1];

    // If scanning is on an overflow page, and hxdel might
    //  empty the page, hxput after hxnext can't just jump to
    //  the right page, because (prevp) is not loaded,
    //  so deleting currp would hard.
    _hxload(locp, currp, SCANNING(hp) && (leng || IS_HEAD(hp->buffer.pgno))
            ? hp->buffer.pgno : locp->head);

    while (1) {
        int     pos, hindpos, skip = 0;
        PAGENO  nextpg = currp->next;

        if (!--loops)
            LEAVE(locp, HXERR_BAD_FILE);

        // Search for the key (an old record to be deleted).
        // If SCANNING: the file is locked, and the matching
        //  record must be there.
        pos = !may_find ? -1
            : !SCANNING(hp) ? _hxfind(locp, currp, locp->hash, recp, &hindpos)
            : currp->pgno == hp->buffer.pgno ? hp->currpos : -1;

        if (pos >= 0) {
            char   *oldp = currp->data + pos;
            COUNT   oldsize = RECSIZE(oldp);
            int     delta = newsize - oldsize;

            locp->ret = RECLENG(oldp);
            may_find = 0;
            assert(!currp->delta);
            currp->delpos = pos;
            currp->delta = delta;

            if (!newsize) {     // hxdel or remove after inserted previously.

                _hxremove(currp, pos, oldsize);
                currp->recs--;
                if (SCANNING(hp))
                    hp->recsize = 0;

            } else if (FITS(hp, currp, delta, 0)) { // replace

                if (delta) {
                    memmove(oldp + newsize, oldp + oldsize,
                            currp->used - pos - oldsize);
                    currp->used += delta;
                    STSH(leng, oldp + sizeof(PAGENO));
                    if (SCANNING(hp))
                        hp->recsize = newsize;
                    DEINDEX(currp); // force indexify
                }

                memcpy(oldp + sizeof(HXREC), recp, leng);
                STAIN(currp);
                newsize = 0;

            } else if (SCANNING(hp)) {
                // At this point we are stuck: if we delete the old copy of
                // the record, we are committed to inserting the new copy
                // somewhere else, but that might require changing links
                // or even growing the file: a NO-NO during a hxnext scan.
                LEAVE(locp, HXERR_BAD_REQUEST);

            } else {            // Delete old version and continue (insert elsewhere).

                _hxremove(currp, pos, oldsize);
                currp->recs--;
            }
        }

        if (currp->used && !IS_HEAD(currp->pgno) && SHRUNK(prevp))
            skip = !_hxshift(locp, locp->head, 0, currp, prevp, NULL);

        // Insert the new record if it fits.
        if (newsize && FITS(hp, currp, newsize, 1)) {

            HXREC   hdr;

            STLG(locp->hash, &hdr.hash);
            STSH(leng, &hdr.leng);
            _hxappend(currp, (char *)&hdr, sizeof hdr);
            _hxappend(currp, recp, leng);
            currp->recs++;
            newsize = 0;
        }
        // If the current page contains only data of OTHER heads 
        // -- and hence, must be at the END of a chain --
        // unlink it from this chain. If the page is empty,
        // unlink it AND put it in the freemap.
        if (IS_HEAD(currp->pgno)) {
            skip = 0;
        } else if (!currp->used) {
            skip = 1;
            _hxputfreed(locp, currp);
            if (SCANNING(hp) && hp->buffer.pgno == currp->pgno)
                hp->buffer.used = 0;
        } else if (currp->next || !SHRUNK(currp)) {
            skip = 0;
        } else if (!skip) {     // If skip not set by _hxshift above...
            char const *rp, *ep;

            FOR_EACH_REC(rp, currp, ep)
                if (locp->head == _hxhead(locp, RECHASH(rp)))
                break;
            skip = rp == ep;    // No recs for locp->head in this tail.
        }
        if (skip)
            LINK(prevp, nextpg);
        else
            SWAP(prevp, currp);

        sync_save(locp, currp);

        if (!newsize && !prevp->next)
            break;

        if (!newsize && !may_find && !SHRUNK(prevp))
            break;

        if (prevp->next) {
            _hxload(locp, currp, prevp->next);
            continue;
        }
        // We are at the end of the chain, and rec not yet inserted.

        // Unlocking is necessary even if tail is not shared;
        //  it may be hp->tail.pgno in some other process.
        if (!FILE_HELD(hp) && !IS_HEAD(prevp->pgno))
            _hxunlock(locp, prevp->pgno, 1);

        // _hxshare/_hxfindfree may update the map (root etc).
        // Split MUST be locked before root, else risk deadlock.
        _hxlockset(locp, BOTH_LOCK);
        if (IS_MMAP(hp))
            _hxremap(locp);
        // At this point assert:
        // - head is locked, split is locked,
        // - head matches hash, npages matches filesize.
        // After locking the split, no other process can change
        // the file size.
        may_find = 0;
        COUNT   need = IS_HEAD(prevp->pgno) ? newsize : 0;

        if (!_hxshare(locp, currp, need)
            && !_hxgetfreed(locp, currp)
            && !_hxfindfree(locp, currp)) {

            // _hxgrow will zero samehead if it splits locp->head.
            PAGENO  samehead = locp->head;

            // _hxgrow will change the file length. A concurrent
            //  hxget/hxdel could miscalculate locp->head as
            //  being the newly-added page.
            _hxlock(locp, locp->npages, 0);
            _hxgrow(locp, currp, need, &samehead);
            DEBUG3("head=%u samehead=%u", locp->head, samehead);
            if (!samehead) {
                _hxputfreed(locp, currp);
                _hxpoint(locp);
                _hxload(locp, currp, locp->head);
                loops = HX_MAX_CHAIN;
                continue;
            }
        }
        // _hxgrow may clobber prevp, so we reload it. Even if
        // prevp->pgno == locp->head, prevp may contain an
        // obsolete copy of the head page. The empty page is
        // always appended to head. _hxshare only returns true
        // if currp is head and currp->next is 0, so it can't
        // clobber it. 

        _hxsave(locp, prevp);
        _hxload(locp, prevp, locp->head);
        LINK(currp, prevp->next);
        LINK(prevp, currp->pgno);
        currp->orig = DATASIZE(hp); // make SHRUNK be true
    }

    assert(!SCANNING(hp) || !may_find);

    sync_save(locp, prevp);
    _hxflushfreed(locp, currp);

    if (HEAD_HELD(locp)) {
        hp->hold = 0;
        locp->mylock = 1;
    }

    LEAVE(locp, locp->ret);
}

//--------------|---------------------------------------------
// sync_save: save a buffer that may affect the page
//  in the persistent hxnext buffer.
static void
sync_save(HXLOCAL * locp, HXBUF * bufp)
{
    if (!DIRTY(bufp))
        return;

    HXFILE *hp = locp->file;

    if (SCANNING(hp) && hp->buffer.pgno == bufp->pgno) {
        hp->buffer.next = bufp->next;
        hp->buffer.used = bufp->used;
        assert(bufp->used >= hp->currpos);
        memcpy(_hxcurrec(hp), bufp->data + hp->currpos,
               bufp->used - hp->currpos);
    }

    _hxsave(locp, bufp);
}

//EOF
