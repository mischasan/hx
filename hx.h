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
// hx.h: interface definitions for HX file access method.
//
// HISTORY
//  08AUG91 MSS    Released

//--------------|---------------------------------------------
#ifndef HX_H
#define HX_H

#include <stdint.h>
#include <stdio.h>      // required by hxfix

#define HXVERSION 0x0100

typedef uint32_t      HXHASH;
typedef struct hxfile HXFILE;

typedef enum {

    HX_READ	= 0,
    HX_UPDATE	= 1,
    HX_RECOVER	= 2,	// check/repair hxfile	
    HX_MMAP	= 4,	// use MMAP'd file access
    HX_MPROTECT	= 8,	// mprotect mmap outside API calls
    HX_FSYNC	=16,	// fsync file at end of API call
    HX_STATIC   =32,    // prevent dl search/load.
    HX_CHECK	= HX_RECOVER+HX_READ,
    HX_REPAIR	= HX_RECOVER+HX_UPDATE

} HXMODE;

// HXRET: enum of return codes from hx api functions.
// READ,LSEEK,... are all for the corresponding syscalls.
//  dlopen is only called by hxopen, which returns NULL
//  on error; so dlopen has no error code of its own.
//  HXERR_MMAP can be for: mmap, munmap, mprotect.
#define	HXERRS \
            _E(BAD_REQUEST) _C \
	    _E(BAD_RECORD)  _C \
	    _E(BAD_FILE)    _C \
            _E(READ)	    _C \
	    _E(LSEEK)	    _C \
            _E(WRITE)	    _C \
            _E(CREATE)	    _C \
            _E(LOCK)	    _C \
	    _E(FTRUNCATE)   _C \
	    _E(MMAP)	    _C \
            _E(FSYNC)	    _C \
	    _E(FOPEN)
#undef	_C
#undef	_E
#define	_C	,
#define	_E(x)	_HXERR_##x
enum { _HXERR=0, HXERRS };
#undef	_E
#define	_E(x)	HXERR_##x = -_HXERR_##x
typedef enum { HXOKAY, HXNOTE, HXERRS } HXRET;

// Caller-supplied functions to be applied to records.
//  "..." values are the (data,leng) passed to hxcreate().
//
// HX_DIFF_FN: returns 0 if two records are the same.
// HX_HASH_FN: returns 32bit hash.
// HX_SAVE_FN: returns the number of bytes required to hold
//              the record in string format (may exceed bufsize).
// HX_LOAD_FN: returns the number of bytes required to hold
//              the record in binary format (max exceed recsize).
// HX_TEST_FN: returns 0 if data is not a valid record of
//              exactly (reclen) bytes.
typedef int (*HX_DIFF_FN)(  char const*arec,  char const*brec,
			    char const*udata, int uleng);

typedef HXHASH (*HX_HASH_FN)(
			    char const*recp,
			    char const*udata, int uleng);

typedef int (*HX_LOAD_FN)(  char *recp,       int recsize,
			    char const *buf,
			    char const*udata, int uleng);

typedef int (*HX_SAVE_FN)(  char const*recp,  int reclen,
			    char *buf,        int bufsize,
			    char const*udata, int uleng);

typedef int (*HX_TEST_FN)(  char const*recp,  int reclen,
			    char const*udata, int uleng);

typedef enum { DIFF, HASH, LOAD, SAVE, TEST } HXFUNC;

#define HX_MIN_PGSIZE    32
#define HX_MAX_PGSIZE 32768
#define	HX_MAX_CHAIN     20
#define	HX_MAX_SHARE    100

typedef struct {
    double	nrecs;
    double	npages;
    // xor-reduced hash of all RECORDS, not keys.
    uint32_t    hash;
    double	head_bytes;
    double	ovfl_bytes;
    double	ovfl_pages;
    // avg # of pg reads per succ/unsucc probe
    double	avg_succ_pages;
    double	avg_fail_pages;
    // histogram of chain lengths
    unsigned	chain_hist[HX_MAX_CHAIN + 1];
	// histogram of multi-chain tails
    unsigned	share_hist[HX_MAX_SHARE + 1];

} HXSTAT;

extern int hxversion;

// (hxdebug,hxproc,hxtime) are set to env values of $HXDEBUG
// etc by the first hxopen call in a process.

// hxdebug: 1,2,3   This prints detailed info for hxfix.
//   Otherwise, it is a source debugging tool. Output format:
//   <detail info...>\t\t...<function><lineno>[<proc>][\t<usec>]
extern int hxdebug;

// "hxproc" adds a process label to debug output in forking code.
// "hxtime" is an unix epoch-secs timestamp. If set, debug output
//      shows elapsed time since (hxtime), in usec.
// "hxlog" is an alternative to stderr for diagnostics.
extern char const *hxproc;
extern double hxtime;
extern FILE *hxlog;

//--------------|---------------------------------------------
// hxbind: supply record-type methods manually.
void hxbind(HXFILE*, HX_DIFF_FN, HX_HASH_FN,
                HX_LOAD_FN, HX_SAVE_FN, HX_TEST_FN);

// hxbuild: bulk-load an empty hxfile.
//  If "inp" is a stream, supplying a nonzero "inpsize"
//  saves a third write/read pass through the input.
HXRET hxbuild(HXFILE*, FILE*, int memlimit, double inpsize);

void hxclose(HXFILE *hp);

// hxcreate: initialize a hx file.
//  "pgsize" must be a power of 2.
//  "udata" is "uleng" bytes of optional data written into the
//  root page of the created file, usable by wrappers using hx.
//  (data,leng) is passed to every "diff" and "hash" call.
HXRET hxcreate(char const*name, int perms, int pgsize,
                char const*udata, int uleng);

// hxerror: name-string for HXRET code
char const*hxerror(HXRET);

// hxfileno: return Unix filehandle, for kevent etc.
int hxfileno(HXFILE const*);

// hxfix: test/repair file.
//  If (FILE*) is supplied, it is used as a scratch file
//  for rebuilding the hxfile, and REPAIR is done.
//  If (FILE*) is NULL, the hx file is checked,
//  but not repaired.
//
//  Returned mode denotes what is safe to do with file,
//  i.e. what won't fail on a BAD_FILE error.
//  HX_UPDATE   - file is clean.
//  HX_READ     - file is safe to read, though hxget
//      may miss some records; and hxnext may return
//      some duplicate records.
//  HX_REPAIR   - file needs to be repaired.
//  hxfix may also return a negative (HXRET) value.
//
// If hxfix() in repair mode returns HX_REPAIR,
//  the file is hopeless.
int hxfix(HXFILE*, FILE*, int pgsize, char const*, int leng);

// hxget: retrieve record, return actual length or 0.
int hxget(HXFILE*, char *recp, int size);

// hxinfo: return udata stored by hxcreate.
//  Returns actual length of udata.
int hxinfo(HXFILE const *hp, char *udata, int usize);

// hxlib: dynamic load of a hx type, returning lib path.
int hxlib(HXFILE*, char const*hxreclib, char**pathp);

// hxmaxrec: return max allowed hxput "leng" value.
int hxmaxrec(HXFILE const*hp);

// hxmode: name-string for hxopen mode bits
char const* hxmode(int mode);

// hxnext: serially retrieve records.
//  Entire file is held with a shared lock.
// NOTE: it is safe to hxput (update) or hxdel the
//  most recent record returned by hxnext.
//  However, if the new record is longer than the
//  old version, "hxput" may return HXERR_BAD_REQUEST.
int hxnext(HXFILE*, char *recp, int size);

// hxopen(name, mode, hash_fn, diff_fn)
// "mode" determines valid functions:
//      HX_READ:   hxget, hxnext
//      HX_UPDATE: all functions except hxfix.
//	HX_RECOVER: hxfix
// "mode" also determines behaviour:
//      HX_FSYNC    fsync file after every API call.
//      HX_MMAP     use mmap'd file
//      HX_MPROTECT mprotect mmap'd file outside API calls.
HXFILE *hxopen(char const*name, HXMODE);

// hxput: insert/update a record.
//  Returns length of replaced record, or zero.
HXRET hxput(HXFILE*, char const*recp, int leng);

// hxrel: release lock by hxhold or hxnext
HXRET hxrel(HXFILE*);

// hxshape: expand or pack a file to a given efficiency.
//  overload=0.0 means NO overflow pages are used.
//  overload>0 means that, on average, an unsuccessful
//  lookup will read (overload + 1) pages.
HXRET hxshape(HXFILE *hp, double overload);

// hxstat: report statistics:
HXRET hxstat(HXFILE*, HXSTAT*);

//--------------|---------------------------------------------
int hx_diff(HXFILE const*, char const *ra, char const *rb);

HXHASH hx_hash(HXFILE const*, char const *rp);

int hx_load(HXFILE const*, char *rp, int size,
                           char const *buf);
int hx_save(HXFILE const*, char const *rp, int recleng,
                                char *buf, int bufsize);
int hx_test(HXFILE const*, char const *rp, int recleng);

//--------------|---------------------------------------------
// hxdel: delete a record.
//  Returns record length on delete, 0 on not-found.
static inline HXRET
hxdel(HXFILE *hp, char const*recp)
{ return hxput(hp, recp, 0); }

// hxhold: hxget with retained lock, whether a
//  record is found or not. Lock is released at the end of
//  a hxput/hxdel with the same key, or at the start of
//  any other HX call that reads/writes the file.
static inline int
hxhold(HXFILE *hp, char *recp, int size)
{
    return size > 0 ? hxget(hp, recp, -size-1) 
                    : HXERR_BAD_REQUEST;
}

// hxpack: pack a file to its minimum size,
//  at the expense of lookup speed.
//  The "shape" parameter is ridiculously large to ensure
//  that an empty file is packed down to minimum size (2 pages).
static inline HXRET
hxpack(HXFILE *hp)
{ return hxshape(hp, 9E9); }

#endif
