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
// hxcheck: Check/repair a hx file.

#include <assert.h>
#include <stdint.h>	// uintptr_t

#include "_hx.h"

//--------------|---------------------------------------------
// Backdoor interface to detailed error stats
#undef  _E
#define _E(x)   #x
char const *hxcheck_namev[] = { ERROR_LIST };
int	    hxcheck_errv[NERRORS];
//--------------|---------------------------------------------
typedef void (LINK_FN)(HXLOCAL*,PAGENO,PAGENO);
typedef void (SAVE_FN)(HXLOCAL*,HXBUF*);

    // nolink,nosave: stubs for HX_CHECK (readonly) mode:
static void nolink(HXLOCAL *locp, PAGENO x, PAGENO y)
    {(void)locp,(void)x,(void)y;}
static void nosave(HXLOCAL *locp, HXBUF *bufp)
    { (void)locp; SCRUB(bufp); }

static void	clear_page(HXLOCAL *locp, HXBUF *bufp);
static int	xor_map(HXLOCAL*, HXBUF *mapp, PAGENO pg, int);
static PAGENO   pghash(PAGENO);

static void check_data_page(HXLOCAL *locp,
				HXBUF	*mapp,	PAGENO pg,
				int	errv[], HXBUF *bufp,
				FILE    *tmpfp);

static void check_map_page(HXLOCAL *locp,
				HXBUF	*mapp,	PAGENO pg,
				int	errv[], PAGENO lastmap,
				int	lastbit);

static void recover(HXLOCAL*, HXBUF const*, FILE*);

#define	REPAIRING(locp)	((locp)->mode == F_WRLCK)
//--------------|---------------------------------------------
int
hxfix(HXFILE *hp, FILE *tmpfp, int pgsize,
	char const *udata, int uleng)
{
    HXLOCAL	loc, *locp = &loc;
    int		ret;
    HXBUF       *bufp, *mapp, *srcp, *dstp;
    PAGENO      *pp, pg, pn, hash, ph, lastmap;
    int		lastbit, leng;
    volatile int badroot = 0;
    char        *recp, *endp;
    LINK_FN     *_cklink;
    SAVE_FN     *_cksave;

    // Note that a corrupt "version" field is a problem.
    // A pre-hxi file will have version (i.e. pgrate) == 4.
    // Anything else can be presumed to be junk.
    // This means there is no way to repair a hxi file that
    // happens to have "0x0004" written onto its version.
    if (!hp || (tmpfp && hp->mode & HX_MMAP)
            || (tmpfp && hp->buffer.pgno)
	    || hp->version == 4 || (uleng && !udata))
	return HXERR_BAD_REQUEST;
#    define BAD(x,p) (DEBUG2("pgno=%u %s",p,hxcheck_namev[x]),\
			    ++hxcheck_errv[x])
#    define VISITED   1   // bit in visit[] to mark loops

    memset(hxcheck_errv, 0, sizeof(hxcheck_errv));

    // Ensure that the file is readable AT ALL.
    if (hp->pgsize < HX_MIN_PGSIZE
	    || hp->pgsize >  HX_MAX_PGSIZE
            || hp->pgsize & (hp->pgsize - 1)) {
        badroot = hp->pgsize = pgsize;
    }

    if (!hp->pgsize)
	return HX_REPAIR;

    if (udata &&
	(hp->uleng-uleng || memcmp(hp->udata, udata, uleng))) {

	hp->udata = realloc(hp->udata, uleng + 1);
	hp->uleng = uleng;
	memcpy(hp->udata, udata, uleng);
	hp->udata[uleng] = 0;
	hxlib(hp, hp->udata, 0);
	badroot = 1;
    }

    // Now that (pgsize,data,uleng) can be trusted...
    ENTER(locp, hp, 0, 2);

    if (!tmpfp)
	locp->mode = F_RDLCK;
    else if (ftruncate(fileno(tmpfp), 0L))
	LEAVE(locp, HXERR_FTRUNCATE);

    _hxlock(locp, 0, 0);
    _hxsize(locp);

    pp = 0;
    bufp = srcp = &locp->buf[0];
    mapp = dstp = &locp->buf[1];
    _cklink = REPAIRING(locp) ? _hxlink : nolink;
    _cksave = REPAIRING(locp) ? _hxsave : nosave;
#    define    _hxlink    _cklink // HACK on PUTLINK

    if (badroot) {                 // FIX UP root page
	HXROOT	    *rp = (HXROOT*)mapp->page;

        _hxload(locp, mapp, 0);
        STSH(hp->pgsize, &rp->pgsize);
        STSH(hp->version, &rp->version);
	mapp->next = LDUL(&mapp->page->next);
	mapp->used = hp->uleng;
	memcpy((char*)(rp + 1), hp->udata, hp->uleng);
	mapp->data[uleng] |= 0x01;
        STAIN(mapp);
        _cksave(locp, mapp);
	BAD(bad_root, 0);
    }

    _hxinitRefs(locp);
    locp->visit = calloc(locp->npages/HXPGRATE+1, sizeof(PAGENO));

    lastmap = locp->npages - 1;
    lastmap = _hxmap(hp, lastmap - lastmap % HXPGRATE, &lastbit);
    for (pg = 0; pg < locp->npages; ++pg) {

        if (IS_MAP(hp, pg)) {
            _cksave(locp, mapp);
            check_map_page(locp, mapp, pg, hxcheck_errv,
			    lastmap, lastbit);
        } else {
            _cksave(locp, bufp);
            check_data_page(locp, mapp, pg, hxcheck_errv,
			    bufp, tmpfp);
        }
    }

    _cksave(locp, bufp);
    _cksave(locp, mapp);
    bufp->pgno = 0; // else CHECK in PUTLINK will ABORT

    // Scan next[] chains, looking for loops,
    // This tries to be more clever than simply running until
    // you hit HX_MAX_CHAIN.
    // xorsumming heads pointing to overflow pages.
    // If we are just CHECKING the file, and we detect loops,
    // then we skip the 'duplicate record' test: the loops
    // have not been fixed!
    for (ph = 1; ph < locp->npages; ++ph) {
        if (IS_HEAD(ph)) {
	    hash = pghash(ph) | VISITED;
	    for (pg = ph; (pn = locp->vnext[pg]); pg = pn) {
		pp = locp->visit + pn/HXPGRATE;
		if (*pp & VISITED) {
		    BAD(bad_loop, pn);
		    PUTLINK(locp, pg, 0);
		    break;
		}
		*pp ^= hash;
	    }
		// A TAIL page is allowed multiple visits:
	    assert(!locp->vnext[pg]);

	    if (pg != ph)
		*pp &= ~VISITED;
	}
    }
    // At this point, all loops in vnext[] have been broken.
    // as well as in the file if we are REPAIRING.
    // Any page with a nonzero visit[] value either contains
    // records unreachable from their head, or has heads 
    // chaining to it for which it has no records.
    //TODO: every page in the chain starting at pn must
    // be unlinked.
    for (pg = 1; pg < locp->npages; ++pg) {
        pn = locp->vnext[pg];
        if (pn && locp->visit[pn/HXPGRATE] & ~VISITED) {
            BAD(bad_refs, pn);
            PUTLINK(locp, pg, 0);
        }
    }

    // Check for duplicate records in adjacent pages.
    if (!hxcheck_errv[bad_loop]) {
	for (ph = 1; ph < locp->npages; ++ph) {
	    if (locp->vnext[ph] && IS_HEAD(ph)) {
		_hxload(locp, srcp, locp->vnext[ph]);
		_hxload(locp, dstp, ph);
                if (hxcheck_errv[bad_index])
                    _hxindexify(locp, dstp, (COUNT*)(dstp->data + DATASIZE(locp->file)) - 1);

		while (1) {
		    FOR_EACH_REC(recp, srcp, endp) {
			if (0 <= _hxfind(locp, dstp, RECHASH(recp), RECDATA(recp), 0))
			    break;
		    }

		    if (recp < endp || !srcp->next) break;
		    SWAP(srcp, dstp);
		    _hxload(locp, srcp, dstp->next);
		}

		if (recp < endp) {
		    BAD(bad_dup_recs, srcp->pgno);
		    pg = dstp->pgno;
		    for (; (pn = locp->vnext[pg]); pg = pn)
			PUTLINK(locp, pg, 0);
		}
	    }
	}
    }

    // The bitmap is now correct. Dump records of
    //  unreferenced nonempty pages, then free those pages.
    for (pg = 0; pg < locp->npages; pg += HXPGRATE) {

        if (IS_MAP(hp, pg)) {
            if (mapp->pgno != pg) {
                _cksave(locp, mapp);
                _hxload(locp, mapp, pg);
            }
        } else if (!VREF(locp,pg) && xor_map(locp,mapp,pg,0)) {
            BAD(bad_orphan, pg);
            if (REPAIRING(locp)) {
                _cksave(locp, bufp);
                _hxload(locp, bufp, pg);
		if (1 != fwrite(bufp->data, bufp->used,
				    1, tmpfp))
		    LEAVE(locp, HXERR_WRITE);
                clear_page(locp, bufp);
                xor_map(locp, mapp, pg, 1);
            }
        }
    }

#    undef BAD   // Search no more for errors
    for (pg = 0; pg < NERRORS; ++pg) {
        if (hxcheck_errv[pg]) {
            DEBUG("%7d x %s", hxcheck_errv[pg],
		    hxcheck_namev[pg]);
        }
    }

#    undef  _E
#    undef  _C
#    define _E(x)    hxcheck_errv[x]
#    define _C    +
    if (!REPAIRING(locp)) {
        LEAVE(locp, FATAL_LIST ? HX_REPAIR
                  : ERROR_LIST ? HX_READ : HX_UPDATE);
    }

    _cksave(locp, bufp);
    _cksave(locp, mapp);

    // Reload dumped records.
    rewind(tmpfp);
    recp = bufp->data;
	// Prevent hxget/hxput from unlocking file
    HOLD_FILE(hp);
    ret = HXOKAY;
    while (fread(recp, sizeof(HXREC), 1, tmpfp)) {
        leng = RECLENG(recp);
	if (leng != (int)fread(recp, 1, leng, tmpfp)) {
	    ret = HXERR_READ;
	    break;
	}
        // Catch (and skip) hx_test failures, rather than
        // have hxput call hx_test and return an error.
	if (!hx_test(hp, recp, leng)) {
	    DEBUG("skip reinsert: %d: %.*s", ret, leng, recp);
	    continue;
	}

        ret = hxget(hp, (char*)(uintptr_t)recp, 0);
        if (!ret)
	    ret = hxput(hp, recp, leng);

        if ((int)ret < 0)
	    break;
    }

    hp->hold = 0;
    LEAVE(locp, ret >= 0 ? HX_UPDATE : ret);
#    undef _hxlink
}

static inline void
check_data_page(HXLOCAL *locp, HXBUF *mapp, PAGENO pg,
	      int errv[], HXBUF *bufp, FILE *tmpfp)
{
    HXFILE	*hp = locp->file;
    PAGENO	xorsum, hash, *pp;
    char	*recp, *endp;
#   define BAD(x,p) (DEBUG2("pgno=%u %s",p,hxcheck_namev[x]), ++errv[x])

    _hxload(locp, bufp, pg);
    if (bufp->next >= locp->npages || IS_HEAD(bufp->next)) {
        BAD(bad_next, pg), LINK(bufp, 0);
    }

    _hxsetRef(locp, bufp->pgno, bufp->next);
    if (bufp->used && (bufp->used < MINRECSIZE ||
                       bufp->used > DATASIZE(hp))) {
        BAD(bad_used, pg);
        clear_page(locp, bufp);
	recover(locp, bufp, tmpfp);
    }

    // Check that (data) is sane:
    // - RECSIZEs are reasonable
    // - RECHASH matches hash(rec)
    // - sum of RECSIZEs is (used)
    // - count of recs is (recs)
    int     recs = 0;
    FOR_EACH_REC(recp, bufp, endp) {
        unsigned   size = RECSIZE(recp);
        if (size < MINRECSIZE || size > (unsigned)(endp-recp)) {
            BAD(bad_rec_size, pg);
            break;
        }

	if (!hx_test(hp, RECDATA(recp), size - sizeof(HXREC))) {
            BAD(bad_rec_test, pg);
	    break;
	}

        hash = hx_hash(hp, RECDATA(recp));
        if (hash != (PAGENO)RECHASH(recp)) {
            BAD(bad_rec_hash, pg);
            break;
        }
        ++recs;
    }

    if (recs != bufp->recs) {
        BAD(bad_recs, pg);
        bufp->recs = recs;
        STAIN(bufp);
    }

    if (recp < endp) {
        bufp->used = recp - bufp->data;
        bufp->recs = recs;
        STAIN(bufp);
	recover(locp, bufp, tmpfp);
    }

    // Check whether this page has multiple heads.
    //  For nonheads, store xor(heads).
    _hxfindHeads(locp, bufp);
    if (!IS_HEAD(pg)) {
        for (xorsum = 0, pp = locp->vprev; *pp;) {
            xorsum ^= pghash(*pp++);
        }

        locp->visit[pg/HXPGRATE] = xorsum & ~VISITED;

    } else if (bufp->used &&
                (locp->vprev[1] || pg != locp->vprev[0])) {
        BAD(bad_head_rec, pg);
        if (REPAIRING(locp)
	    && 1 != fwrite(bufp->data, bufp->used, 1, tmpfp))
	    LEAVE(locp, HXERR_WRITE);

        clear_page(locp, bufp);
    }

    // From here on, nothing changes bufp->used
    if (!bufp->used && bufp->next) {
        BAD(bad_free_next, pg);
        BUFLINK(locp, bufp, 0);
    }

    // Check that bytes beyond [used] are correct.
    int   hsize = HIND_SIZE(hp, bufp);
    COUNT *act = (COUNT*)(bufp->data + DATASIZE(hp)) - hsize;
    COUNT exp[hsize];
    _hxindexify(locp, bufp, exp + hsize - 1);
    if (memcmp(act, exp, hsize * sizeof(COUNT))) {
        BAD(bad_index, pg);
        if (tmpfp) {
            memcpy(act, exp, hsize*sizeof(COUNT));
            STAIN(bufp);
        }
    }

    // Check whether page is correct in bitmap
    if (!IS_HEAD(pg)
            && !bufp->used != !xor_map(locp,mapp,pg,0)) {
        BAD(bufp->used ? bad_free_bit : bad_used_bit, pg);
        xor_map(locp, mapp, pg, 1);
    }
#   undef    BAD
}

static inline void
check_map_page(HXLOCAL *locp, HXBUF *mapp, PAGENO pg,
	     int errv[], PAGENO lastmap, int lastbit)
{
    HXFILE	*hp = locp->file;

#    define BAD(x,p) (DEBUG2("pgno=%u %s",p,hxcheck_namev[x]), ++errv[x])
    _hxload(locp, mapp, pg);

    if (pg && (mapp->next || mapp->used)) {
        BAD(bad_map_head, pg);
        mapp->next = mapp->used = 0;
        STAIN(mapp);
    }

    if (mapp->used >= DATASIZE(hp) ||
	    !(mapp->data[mapp->used] & 0x01)) {
        BAD(bad_map_self, pg);
        mapp->data[mapp->used] |= 0x01;
        STAIN(mapp);
    }

    if (pg == lastmap) {
        BYTE    mask = -2 << (lastbit & 7);
        int     pos = lastbit >> 3;

        if (mask & mapp->data[pos]) {
            mapp->data[pos] &= ~mask;
            STAIN(mapp);
        } else {
            do ++pos;
            while ((unsigned)pos < DATASIZE(hp)
                && !mapp->data[pos]);
        }

        if ((unsigned)pos < DATASIZE(hp)) {
            BAD(bad_overmap, pg);
	    memset(mapp->data+pos, 0, DATASIZE(hp)-pos);
            STAIN(mapp);
        }
    }
#    undef BAD   // Search no more for errors
}

static void
clear_page(HXLOCAL *locp, HXBUF *bufp)
{
    bufp->orig = DATASIZE(locp->file);
    bufp->used = bufp->recs = 0;
    STAIN(bufp);
    BUFLINK(locp, bufp, 0);
}


static void
recover(HXLOCAL *locp, HXBUF const *bufp, FILE *fp)
{
    HXFILE	*hp = locp->file;

    if (!fp || !hp->test)
	return;

    char const	*recp = bufp->data + bufp->used;
    char const	*endp = bufp->data + DATASIZE(hp);

    while (recp < endp) {

	if (!RECHASH(recp) || !RECLENG(recp)) {

	    recp += sizeof(PAGENO) - 1;

	} else if (RECSIZE(recp) > (unsigned)(endp - recp)
		   || !hx_test(hp, RECDATA(recp), RECLENG(recp))
		   ||  hx_hash(hp, recp) != RECHASH(recp)) {
	    ++recp;

	} else {
	    if (1 != fwrite(recp, RECSIZE(recp), 1, fp))
		LEAVE(locp, HXERR_WRITE);

	    recp += RECSIZE(recp);
	}
    }
}

static int
xor_map(HXLOCAL *locp, HXBUF *mapp, PAGENO pg, int flip)
{
    int         pos, mask;
    PAGENO      mappg = _hxmap(locp->file, pg, &pos);
    (void)mappg;
    assert(mappg == mapp->pgno);
    mask = 1 << (pos & 7);
    pos >>= 3;
    if (flip)
	mapp->data[pos] ^= mask, STAIN(mapp);

    return mapp->data[pos] & mask;
}

// pghash: hash a pageno; used in detecting linkage errors.
static PAGENO
pghash(PAGENO pg)
{
    PAGENO	hash = 2166136261U;

    hash = (hash ^ (pg & 0xFF)) * 16777619; pg >>= 8;
    hash = (hash ^ (pg & 0xFF)) * 16777619; pg >>= 8;
    hash = (hash ^ (pg & 0xFF)) * 16777619; pg >>= 8;
    hash = (hash ^ (pg & 0xFF)) * 16777619;
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return hash;
}
//EOF
