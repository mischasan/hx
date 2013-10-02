// Copyright (C) 2009-2013 Mischa Sandberg <mischasan@gmail.com>
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
// IF YOU ARE UNABLE TO WORK WITH GPL2, CONTACT ME.
//-------------------------------------------------------------------

//
// _hx.h: implementation definitions for HX access method.

#ifndef    _HX_H
#define    _HX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <setjmp.h>

#if defined(__FreeBSD__)
#   include <sys/endian.h>      // byteswap
#   define  bswap_32(x) bswap32(x)
#   define regargs
#elif defined(__linux__)
#   include <byteswap.h>
#   include <endian.h>          // __BYTE_ORDER
#   define regargs __attribute__((fastcall))
#else
#   error "need byteswap*32 intrinsic for HXHASH"
#endif

#ifdef _WIN32
#   include <io.h>
#endif
#ifndef O_BINARY
#   define    O_BINARY    0
#endif

#include <sys/mman.h>
#ifndef MAP_NOCORE              //FreeBSD-ism
#   define  MAP_NOCORE  0
#endif

#include "hx.h"
typedef uint8_t BYTE;
typedef uint16_t COUNT;
typedef uint32_t PAGENO;

typedef int (*cmpfn_t) (const void *, const void *);

// LDSS, LDUS, LDUL, STSH and STLG are machine-indep macros
// to load/store signed-short, unsigned-short, and long
// LSB-first values from (potentially non-word-unaligned)
// locations in memory.
#ifdef __sparc__
#   define LDSS(cp) \
         (((int16_t)*(1+(char const*)(cp)) << 8) \
            | *(BYTE const*)(cp))
#   define LDUS(cp)     ((uint16_t)LDSS(cp))
#   define LDUL(cp) \
            (((uint32_t)LDSS(2+(char const*)cp) << 16) | LDUS(cp))
#   define STSH(sh,cp)  (*(1+(char*)(cp))=(char)((sh)>>8), \
                         *(char*)(cp)=(char)(sh))
#   define STLG(lg,cp)  (STSH((uint16_t)(lg),(cp)),\
                        STSH((uint32_t)(lg)>>16, 2+(char*)(cp)))
#else // LITTLE_ENDIAN UNALIGNED
#   define LDSS(cp)     (*(uint16_t const*)(cp))
#   define LDUS(cp)     (*(uint16_t const*)(cp))
#   define LDUL(cp)     (*(uint32_t const*)(cp))
#   define STSH(sh,cp)  (*(uint16_t*)(cp) = sh)
#   define STLG(lg,cp)  (*(uint32_t*)(cp) = lg)
#endif

#ifdef __GNUC__
#   define PACKED	    __attribute__((packed))
#   define ALIGNED(size)    __attribute__((__aligned__(size)))
#else
#   error "Need a non-GCC way to specify attributes"
#endif

enum { HXPGRATE = 4 };

typedef struct {

    PAGENO  next;               // Linked-list per cluster, ends with 0.
    COUNT   used;               // BYTEs-used of data[]
    COUNT   recs;
    char    data[0];

} PACKED HXPAGE;

// Special case page layout: ROOT page [0].
typedef struct {

    COUNT   pgsize;             // size of each page
    COUNT   version;            // #(data pages) per ovfl page
    COUNT   uleng;              // BYTEs-used of data[]
    COUNT   unused;
} PACKED HXROOT;

// In file, record prefix is (hash,leng) in LSB-first form:

typedef struct {
    HXHASH  hash;
    COUNT   leng;
} PACKED HXREC;

#define MINRECSIZE      (sizeof(HXREC) + 1)

#define RECHASH(rp) (HXHASH)(LDUL(rp))
//static inline HXHASH   RECHASH(void const *rp) { return LDUL(rp); }

static inline unsigned
RECLENG(void const *rp)
{
    return LDUS(sizeof(PAGENO) + (char const *)rp);
}

static inline unsigned
RECSIZE(void const *rp)
{
    return sizeof(HXREC) + RECLENG(rp);
}

static inline char const *
RECDATA(void const *rp)
{
    return sizeof(HXREC) + (char const *)rp;
}

typedef struct {
    PAGENO  pgno;
    COUNT   used, recs;
} PGINFO;

typedef struct {
    PAGENO  pgno;               // address of this page
    PAGENO  next;               // native format next
    COUNT   used;               // native format used
    COUNT   recs;               // native format recs
    COUNT   orig;               // value of "used" at time page is read
    COUNT   flag;               // 0 or 1|2 for CLEAN or DIRTY.

    COUNT  *hind;               // points to LAST 16 bits of page->data
    int     hsize;              // size of hind[]
    int     hmask;              // smallest (2**N - 1) >= hsize.

    HXPAGE *page;
#   define   data    page->data

    // Info required for increment _hxindexify:

    int     delpos;             // offset of record deleted or replaced by hxput.
    int     delta;              // change in length deleted or replaced by hxput.

    // behind: hind[] entries that were overwritten by replace or append.
    //  behind is __m128i-aligned and -sized, so that SSE2 ops
    //  can be used to adjust it for (delpos,delta).
    // When nbehind reaches 16, _hxindexify does a full reindex.
    int     nbehind;
    COUNT   behind[16] __attribute__ ((aligned(16)));

} HXBUF;

enum { CLEAN = 0, DIRTY_DATA = 1, DIRTY_LINK = 2, DIRTY_INDEX = 4 };

#define SHRUNK(bp)  ((bp)->used < (bp)->orig)
#define SCRUB(bp)   ((bp)->flag  = CLEAN)
#define STAIN(bp)   ((bp)->flag |= DIRTY_DATA)
#define BLINK(bp)   ((bp)->flag |= DIRTY_LINK)
#define DIRTY(bp)   ((bp)->flag != CLEAN)
#define LINK(bp,pg) ((bp)->next = pg, BLINK(bp))
#define DEINDEX(bp) ((bp)->flag |= DIRTY_INDEX)
#define UNDEXED(bp) ((bp)->flag & DIRTY_INDEX)
#define SWAP(a,b)   do {typeof(a)x = a; a = b; b = x;} while(0)

#define FOR_EACH_REC(rp,bp,ep) \
            for (rp = (bp)->data, ep = rp + (bp)->used;\
                 rp < ep; rp += RECSIZE(rp))
//  HEAD - just lock head page. Called by hxget and hxdel.
//  HIGH - head page is locked and head > split.
//  BOTH - head AND split page are locked.
typedef enum { NONE_LOCK = 0, HEAD_LOCK = 1, HIGH_LOCK = 2, BOTH_LOCK = 3 } LOCKPART;

struct hxfile {

    HXMODE  mode;
    int     fileno;             // file descriptor
    COUNT   pgsize;             // page size
    COUNT   version;            // data-to-overflow ratio
    COUNT   uleng;              // Length of udata, in BYTEs
    char   *udata;              // User-defined data

    void   *dlfile;             // dlloaded record type methods
    HX_DIFF_FN diff;
    HX_HASH_FN hash;
    HX_LOAD_FN load;
    HX_SAVE_FN save;
    HX_TEST_FN test;

    off_t   mlen;
    char   *mmap;

    // hxbuild,hxupd:
    HXBUF   tail;               // Not really a buf; FITS uses (pgno,used,recs)

    // hxhold,hxnext:
    PAGENO  hold;               // See HOLD_HEAD, etc.

    // _hxlock,_hxunlock:
    short   locked;             // flags for wide locks
#   define  MAXLOCKS (HX_MAX_CHAIN + HXPGRATE*2 + 1)
    PAGENO  lockv[MAXLOCKS + 1];

    // _hxlockset:
    LOCKPART lockpart;
    //int         lockpart; // 0: not locked
    // 1: head locked
    // 2: head locked, split<head
    // 3: head+split locked.
    // hxnext:
    HXBUF   buffer;
    PAGENO  head;               // of current chain
    COUNT   currpos;            // of last rec returned
    COUNT   recsize;            // of last rec returned
    // IS_MAP:
    PAGENO  map1;               // pgno of first map > 0 (perhaps not yet allocated)
};

// max bufs required by any one op
//  hxput requires (up to) 3.
//  hxfix requires HX_MAX_CHAIN+1.
#define MAXHXBUFS (HX_MAX_CHAIN  + 1)

// HXLOCAL struct is a common area to each top-level HX proc,
//  such as "hxget", and all routines it calls.
enum { LOCKED_ROOT = 1, LOCKED_BODY = 2, LOCKED_FILE = 3, LOCKED_BEYOND = 4 };

typedef struct {

    HXFILE *file;
    PAGENO  npages;             // curr. # of pages in file
    PAGENO  dpages;             // _hxd2f(npages)
    PAGENO  mask;               // (min power of 2 >= dpages) - 1
    HXHASH  hash;               // key hash of target record
    PAGENO  head;               // chain head page for key hash
    short   mode;               // lock mode for this op
    short   mylock;             // 1 if this call set a lock
    short   changed;            // set if file changes; for fsync
    HXBUF   buf[MAXHXBUFS];

    // hxput:
    PAGENO  freed;              // Pending freed page

    // hxref:
    PAGENO *vnext;              // (next) field of each page
    PAGENO *vprev;              // results of findHeads/findRefs
    COUNT  *vrefs;              // refcount for each ovfl page

    // hxpack:
    PGINFO *vtail;              // tail pages to merge

    // hxfix:
    PAGENO *visit;              // xor'd head(s) of ovfl pages

    // hxbuild:
    int     memsize;            // XXX size_t
    char   *membase;
    char   *mem;                // membase aligned(DISK_PGSIZE)
    void   *recv;               // sortable vector of {HXREC*,PAGENO}
    int     mapsize;
    char   *tmpmap;             // mmap-ed partition file
    int     nfps;
    FILE   *fp[3];

    // _hxenter/_hxleave
    HXRET   ret;
    jmp_buf jmp;

} HXLOCAL;

//-----|-------|-------|-------|-------|-------|-------|-------|
// A fn with (HXLOCAL const*) cannot error-out.
void    _hxaddlock(HXFILE *, PAGENO) regargs;
void    _hxalloc(HXLOCAL *, PAGENO, int bitval) regargs;
void    _hxappend(HXBUF *, char const *, COUNT) regargs;
char   *_hxblockstr(HXFILE *, char *) regargs;
HXRET   _hxcheckbuf(HXLOCAL const *, HXBUF const *) regargs;
void    _hxdebug(char const *func, int line, char const *fmt, ...) regargs;
void    _hxenter(HXLOCAL *, HXFILE *, char const *, int nbufs) regargs;
int     _hxfind(HXLOCAL *, HXBUF const *, HXHASH, char const *, int *hindp) regargs;
void    _hxflushfreed(HXLOCAL *, HXBUF *) regargs;
int     _hxgetfreed(HXLOCAL *, HXBUF *) regargs;
void    _hxputfreed(HXLOCAL *, HXBUF *) regargs;
void    _hxfresh(HXLOCAL const *, HXBUF *, PAGENO) regargs;
int     _hxfindfree(HXLOCAL *, HXBUF *) regargs;
void    _hxgrow(HXLOCAL *, HXBUF *, COUNT, PAGENO *) regargs;
PAGENO  _hxhead(HXLOCAL const *locp, HXHASH) regargs;
char   *_hxheads(HXLOCAL *, HXBUF const *, char *outstr) regargs;
COUNT  *_hxindex(HXFILE const *, HXBUF const *, HXHASH) regargs;
int     _hxindexed(HXLOCAL *, HXBUF const *) regargs;
void    _hxindexify(HXLOCAL const *, HXBUF *, COUNT *) regargs;
int     _hxislocked(HXLOCAL const *, PAGENO) regargs;
HXRET   _hxleave(HXLOCAL *) regargs;
void    _hxlink(HXLOCAL *, PAGENO pg, PAGENO nextpg) regargs;
void    _hxload(HXLOCAL *, HXBUF *, PAGENO) regargs;
void    _hxlock(HXLOCAL *, PAGENO, COUNT) regargs;
void    _hxlockset(HXLOCAL *, LOCKPART) regargs;
void    _hxlockup(HXLOCAL *);
PAGENO  _hxmap(HXFILE const *, PAGENO, int *bitpos) regargs;
void    _hxmove(HXLOCAL const *, HXBUF *, PAGENO) regargs;
PGINFO  _hxpginfo(HXLOCAL *, PAGENO) regargs;
void    _hxpoint(HXLOCAL *) regargs;
void    _hxprbuf(HXLOCAL const *, HXBUF const *, FILE *) regargs;
void    _hxprfile(HXFILE const *);
void    _hxprloc(HXLOCAL const *) regargs;
void    _hxprlox(HXFILE *) regargs;
void    _hxread(HXLOCAL *, off_t, void *, int) regargs;
void    _hxremap(HXLOCAL *) regargs;
void    _hxremove(HXBUF *, COUNT pos, COUNT size) regargs;
void    _hxresize(HXLOCAL *, PAGENO) regargs;
void    _hxsave(HXLOCAL *, HXBUF *) regargs;
int     _hxshare(HXLOCAL *, HXBUF *, COUNT need) regargs;
int     _hxshift(HXLOCAL const *, PAGENO lo, PAGENO hi,
                 HXBUF * srcp, HXBUF * lowerp, HXBUF * upperp) regargs;
void    _hxsize(HXLOCAL *) regargs;
void    _hxsplits(HXFILE *, PAGENO *, PAGENO);
int     _hxtemp(HXLOCAL *, char *vbuf, int vbufsize) regargs;
void    _hxunlock(HXLOCAL *, PAGENO start, PAGENO npages) regargs;

// REF functions are used by hxfix/hxshape/hxstat and diag in _hxsave.
void    _hxinitRefs(HXLOCAL *) regargs;
int     _hxfindHeads(HXLOCAL const *, HXBUF const *) regargs;
void    _hxfindRefs(HXLOCAL *, HXBUF const *, PAGENO) regargs;
PAGENO  _hxgetRef(HXLOCAL const *, PAGENO head, PAGENO tail) regargs;
void    _hxsetRef(HXLOCAL *, PAGENO pg, PAGENO next) regargs;

// VREF is used by (hxfix, hxshape) as an lvalue.
#define VREF(locp,pg) ((locp)->vrefs[(pg)/HXPGRATE])

#define ENTER(locp, hp, rec, nbufs) \
        memset(locp, 0, sizeof(HXLOCAL)); \
        if (setjmp((locp)->jmp)) return _hxleave(locp); \
        else _hxenter(locp, hp, rec, nbufs)

#define LEAVE(locp, eret) \
    do {if (0 > ((locp)->ret = eret)) DEBUG(hxerror(eret));\
        longjmp((locp)->jmp, 1); } while(0)

// This MUST be a #define, so hxfix can override _hxlink
#define PUTLINK(locp, pg, next) \
        (_hxlink(locp, pg, next), _hxsetRef(locp, pg, next))

//--------------|---------------------------------------------
static inline char *_hxcurrec(HXFILE const *hp)
{
    return hp->buffer.data + hp->currpos;
}

static inline PAGENO
_hxd2f(PAGENO pg)
{
    return pg * HXPGRATE / (HXPGRATE - 1) + 1;
}

static inline PAGENO
_hxf2d(PAGENO pg)
{
    return (pg - 1) - (pg - 1) / HXPGRATE;
}

// Used by _FITS below
static inline int
MIN_INDEX_BYTES(int nrecs)
{
    return (nrecs + (nrecs + 7) / 8) * sizeof(COUNT);
}

static inline void
BUFLINK(HXLOCAL * locp, HXBUF * bufp, PAGENO pg)
{
    LINK(bufp, pg), _hxsetRef(locp, (bufp)->pgno, pg);
}

static inline int
BUFNUM(HXLOCAL const *locp, HXBUF const *bufp)
{
    return bufp == &locp->file->buffer ? 9 : bufp - locp->buf;
}

static inline unsigned
DATASIZE(HXFILE const *hp)
{
    return hp->pgsize - sizeof(HXPAGE);
}

static inline int
FILE_HELD(HXFILE const *hp)
{
    return hp->hold == HXPGRATE;
}  

//FITS and _FITS takes signed (int) used, because hxput tests shrinkage (dused<0)
static inline unsigned
_FITS(HXFILE const *hp, int sused, int srecs, int dused, int drecs)
{
    return (sused + dused) + MIN_INDEX_BYTES(srecs + drecs) <= (int)DATASIZE(hp);
}

static inline unsigned
FITS(HXFILE const *hp, HXBUF const *bp, int dused, int drecs)
{
    return dused < 0 || _FITS(hp, bp->used, bp->recs, dused, drecs);
}

static inline int
HEAD_HELD(HXLOCAL const *locp)
{
    return locp->file->hold == locp->head;
}

static inline COUNT *
HIND_BASE(HXFILE const *hp, HXBUF * bufp)
{
    return (COUNT *) (bufp->data + DATASIZE(hp)) - 1;
}

static inline COUNT const *
HIND_BASE_C(HXFILE const *hp, HXBUF const *bufp)
{
    return (COUNT *) (bufp->data + DATASIZE(hp)) - 1;
}

static inline COUNT
HIND_POS(HXBUF const *bufp, HXHASH hash)
{
    COUNT   ret = hash & bufp->hmask;

    return ret < bufp->hsize ? ret : ret & (bufp->hmask >> 1);
}

static inline int
HIND_SIZE(HXFILE const *hp, HXBUF const *bp)
{
    return (DATASIZE(hp) - bp->used) / sizeof(COUNT);
}

static inline void
HOLD_FILE(HXFILE * hp)
{
    hp->hold = HXPGRATE, hp->locked = LOCKED_FILE;
}

static inline void
HOLD_HEAD(HXLOCAL * locp)
{
    locp->file->hold = locp->head;
}

static inline int
IMIN(int a, int b)
{
    return a < b ? a : b;
}

static inline int
IS_HEAD(PAGENO pg)
{
    return pg % HXPGRATE;
}

static inline int
IS_MAP(HXFILE const *hp, PAGENO pgno)
{
    return pgno == 0 || pgno == hp->map1
        || (pgno > hp->map1 && 0 == (pgno + 8 * HXPGRATE * hp->uleng)
            % (8 * HXPGRATE * DATASIZE(hp)));
}

static inline int
IS_MMAP(HXFILE const *hp)
{
    return hp->mode & HX_MMAP;
}

static inline void
MAP_BUF(HXFILE const *hp, HXBUF * bufp)
{
    bufp->page = (HXPAGE *) & hp->mmap[(off_t) bufp->pgno * hp->pgsize];
}

// _hxmask: return a bitmask covering all bits of [0..num-1].
//  For example, mask(12) = 15, mask(32) = 31.
static inline PAGENO
MASK(PAGENO x)
{
    if (!x--)
        return 0;
#if !defined(__BSD_VISIBLE) && defined(__x86_64)
  asm("bsrl %0,%0": "=a"(x):"a"(x));
    x = ~(-1 << (x + 1));
#else
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#endif
    return x;
}

static inline int
MAX_RECS(HXFILE const *hp)
{
    return DATASIZE(hp) / MINRECSIZE;
}

static inline PAGENO
NEXT_MAP(HXFILE const *hp, PAGENO mappg)
{
    return mappg + 8 * HXPGRATE * (DATASIZE(hp) - (mappg ? 0 : -hp->uleng));
}

static inline int
OK_VERSION(COUNT version)
{
    return !(0xFF00 & (version ^ hxversion))
        && (0x00FF & version) <= (0x00FF & hxversion);
}

static inline HXHASH
REV_HASH(HXHASH x)
{
    return bswap_32(x);
}

static inline unsigned
ROOT_SIZE(HXFILE const *hp)
{
    return DATASIZE(hp) - hp->uleng;
}

static inline int
SCANNING(HXFILE const *hp)
{
    return ! !hp->buffer.page;
}

//XXX
static inline PAGENO
SPLIT_HI(HXLOCAL *locp, PAGENO pg)
{ // is locp->mask always correct when SPLIT_HI is called?
    pg = _hxf2d(pg);
    PAGENO delta = (locp->mask + 1) >> (pg <= locp->mask);
    return _hxd2f(pg + delta);
}

static inline PAGENO
SPLIT_LO(HXLOCAL *locp, PAGENO pg)
{
    (void)locp;
    pg = _hxf2d(pg);
    return _hxd2f(pg - (MASK(pg) + 1)/2);
}

static inline PAGENO
SPLIT_PAGE(HXLOCAL const *locp)
{
    return _hxd2f(locp->dpages - (locp->mask + 1)/2);   //XXX -1???
}

#define TWIXT(x,a,z) ((unsigned)((x)-(a)) <= (unsigned)(z)-(a))
//--------------|---------------------------------------------
// "hxcrash" makes the (n)th _hxsave call abort()
// Used to create corrupt files to test HX_REPAIR.
extern int hxcrash;

#define DEBUG(...)  (hxdebug>=1?_hxdebug(__PRETTY_FUNCTION__,__LINE__,__VA_ARGS__):0)
#define DEBUG2(...) (hxdebug>=2?_hxdebug(__PRETTY_FUNCTION__,__LINE__,__VA_ARGS__):0)
#define DEBUG3(...) (hxdebug>=3?_hxdebug(__PRETTY_FUNCTION__,__LINE__,__VA_ARGS__):0)
#define DEBUG4(...) (hxdebug>=4?_hxdebug(__PRETTY_FUNCTION__,__LINE__,__VA_ARGS__):0)
//--------------|---------------------------------------------

//---- ERRORS THAT MAKE A FILE READABLE (WITH POSSIBLE DUP RECS).
// bad_dup_recs Two records with the same key occur in two
//                  adjacent pages in a chain.
// bad_free_bit A map bit is zero for an overflow page that
//                  some other page references (i.e is in use)
// bad_free_next A page has used=0 and next>0.
// bad_head_rec A head page contains recs for some other head.
// bad_map_head A (non-root) map page has nonzero (next,used).
// bad_map_self Bit 0 of a map page is 0; the map page doesn't
//                  indicate that it itself is not free.
// bad_orphan   A non-empty overflow page is not referenced
//                  by any other page.
// bad_overmap  There is nonzero junk beyond the bit pos of
//                  the last overflow page (in the last map page).
// bad_overused There are nonzero bytes beyond (used) in a data page.
// bad_recs     The (recs) field is invalid or disagrees with (used).
//              (used) trumps (recs).
// bad_rec_test A record failed hx_test()
// bad_refs Some (next) links to a page that it should not.
// bad_root Root page is bad in any way other than map error.
// bad_used_bit Opposite of "bad_free_bit".

//---- ERRORS THAT MAKE A FILE UNREADABLE:
// bad_loop An overflow chain contains a loop.
// bad_next (next) is not 0 and not an overflow pgno.
// bad_rec_hash rec.hash in file does not match hx_hash(rec).
//              This means the record is likely corrupt.
// bad_rec_size Rec size is 0 or goes beyond (used).
// bad_used Overflow page has (used)>0 but its map bit is 0,
//                  or vice versa.

#define FATAL_LIST \
    _E(bad_index)_C     _E(bad_loop)_C	    _E(bad_next)_C  \
    _E(bad_rec_hash)_C  _E(bad_rec_size)_C  _E(bad_used)

#define ERROR_LIST  \
    _E(bad_dup_recs)_C  _E(bad_free_bit)_C  _E(bad_free_next)_C\
    _E(bad_head_rec)_C	_E(bad_map_head)_C  _E(bad_map_self)_C \
    _E(bad_orphan)_C	_E(bad_overmap)_C   _E(bad_recs)_C     \
    _E(bad_rec_test)_C  _E(bad_refs)_C      _E(bad_root)_C     \
    _E(bad_used_bit)_C  FATAL_LIST

#undef  _E
#undef  _C
#define _E(x)   x
#define _C      ,
enum { ERROR_LIST, NERRORS };
extern char const *hxcheck_namev[];
extern int hxcheck_errv[];

#endif //_HX_H
