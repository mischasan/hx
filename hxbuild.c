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
// hxbuild: populate an empty file with a stream of records.

#include <assert.h>
#include <stdint.h>             // uintptr_t
#include <sys/stat.h>

#include "_hx.h"
#include "util.h"

typedef struct {
    HXREC  *recp;
    PAGENO  head;
} REC;
typedef struct {
    off_t   nbytes;
    int     nrecs;
} PART;

static void _flush(HXLOCAL *, int vbufsize, int i, off_t nbytes);
static void _parse(HXLOCAL *, char *, int leng, HXREC *, int size);
static void _split(HXLOCAL *, PART *, int nparts, int fd, FILE *);
static void _store(HXLOCAL *, char *, int nrecs, PAGENO *);

// Sort records by head (for placement in chains) and by
//  hash (to detect duplicate keys).
static int
cmprec(REC const *a, REC const *b)
{
    int     cmp = (int)a->head - (int)b->head;

    return cmp ? cmp : (int)RECHASH(a->recp) - (int)RECHASH(b->recp);
}

static int
recdiff(HXFILE * hp, HXREC * a, HXREC * b)
{
    return RECHASH(a) != RECHASH(b)
        || hx_diff(hp, RECDATA(a), RECDATA(b));
}

#define  TICK(x) double x = tick()

// MINMEM eliminates some edge cases
#define        MINMEM            (1 << 20)
#define        DISK_PGSIZE (1 << 13)
#define        STD_BUFSIZE (DISK_PGSIZE * 4)

HXRET
hxbuild(HXFILE * hp, FILE * inp, int memlimit, double size)
{
    HXLOCAL loc, *locp = &loc;
    int     i;
    int     nrecs = 0;          // recs parsed from inpf
    double  nbytes = 0;         // bytes written to fp[0]
    double  inpseen = 0;        // bytes read from inpf
    int     memlen = 0;         // bytes in mem[]

    if (!hp || !inp || memlimit < MINMEM || !hp->load
        || !hp->test || !(hp->mode & HX_UPDATE))
        return HXERR_BAD_REQUEST;

    ENTER(locp, hp, NULL, 1);
    int const maxrec = DATASIZE(hp);
    int const maxinp = 3 * maxrec;  // a guess

    // Make copies of args, else gcc whinges about longjmp.
    FILE   *inpf = inp;
    double  inpsize = size;
    HXBUF  *bufp = &locp->buf[0];
    struct stat sb;

    if (!inpsize && !fstat(fileno(inpf), &sb))
        inpsize = sb.st_size;

    // Read inpfile, transform recs, write fp[0]

    // fp[0]: HXRECs that overflow the load into mem[].
    //        If the input is a stream (not a file), and no size
    //        is given, fp[0] gets the entire input.
    // fp[1]: HXRECs that overflow _split or _store.
    //        They must be added with hxput, after bulk insert.

    char    vb[2][STD_BUFSIZE] ALIGNED(DISK_PGSIZE);

    _hxtemp(locp, vb[0], sizeof vb[0]);
    _hxtemp(locp, vb[1], sizeof vb[1]);

    locp->memsize = memlimit;
    locp->membase = locp->mem = malloc(memlimit + DISK_PGSIZE);
    locp->mem += (DISK_PGSIZE - 1) & -(uintptr_t) locp->mem;

    char    inpbuf[maxinp];

    TICK(t0);
    while (fgets(inpbuf, maxinp, inpf)) {
        HXREC  *rp = (HXREC *) (locp->mem + memlen);
        int     len = strlen(inpbuf);

        inpseen += len;
        nrecs++;
        _parse(locp, inpbuf, len, rp, maxrec);

        memlen += RECSIZE(rp);
        if (memlen <= locp->memsize - maxrec)
            continue;

        if (inpsize)            // We know enough to guess nparts
            break;

        len = memlen & -DISK_PGSIZE;
        memlen &= DISK_PGSIZE - 1;
        nbytes += len;
        if (!fwrite(locp->mem, len, 1, locp->fp[0]))
            LEAVE(locp, HXERR_WRITE);

        memmove(locp->mem, locp->mem + len, memlen);
    }

    if (ferror(inpf))
        LEAVE(locp, HXERR_READ);
    if (!inpseen)               // Empty input
        LEAVE(locp, HXOKAY);

    if (!fwrite(locp->mem, memlen, 1, locp->fp[0]))
        LEAVE(locp, HXERR_WRITE);
    nbytes += memlen;

    DEBUG("analyze:%.3fs nrecs:%d nbytes:%.0f inpsize:%.0f",
          tick() - t0, nrecs, nbytes, inpsize);

    // Extrapolate nbytes and nrecs
    if (!feof(inpf)) {
        nbytes *= inpsize / inpseen;
        nrecs *= inpsize / inpseen;
    }

    _hxlock(locp, 0, 0);
    _hxresize(locp, 1);         // Retain udata[].
    if (!inpseen) {
        _hxresize(locp, 2);
        LEAVE(locp, HXOKAY);    // Empty input
    }
    // (nbytes) includes HXREC overhead (6 bytes per rec).
    // The average space wasted per page is half a record.
    // Hash index takes (9*nrecs+7)/8 COUNT fields.
    // Resize the hxfile for all input to fit in head pages.

    _hxresize(locp,
              1 +
              _hxd2f((nbytes + MIN_INDEX_BYTES(nrecs)) / (DATASIZE(hp) -
                                                          nbytes / nrecs /
                                                          2)));
    locp->head = 0;             // disable rec_hash test in _hxcheckbuf.
    PAGENO  ovfl = HXPGRATE;

    if (feof(inpf) && nbytes <= memlen) {

        locp->recv = malloc((nrecs + 1) * sizeof(REC));
        _store(locp, locp->mem, nrecs, &ovfl);

    } else {

        int     fd = _hxtemp(locp, NULL, 0);
        int     nparts = (nbytes - 1) / locp->memsize + 1;
        PART    partv[nparts];

        TICK(t2);
        _split(locp, partv, nparts, fd, inpf);
        TICK(t3);

        free(locp->membase);
        locp->membase = locp->mem = 0;

        for (i = nrecs = 0; i < nparts; ++i)
            if (nrecs < partv[i].nrecs)
                nrecs = partv[i].nrecs;
        locp->recv = malloc((nrecs + 1) * sizeof(REC));

        for (i = 0; i < nparts; ++i) {
            locp->mapsize = partv[i].nbytes;
            locp->tmpmap = mmap(NULL, locp->memsize, PROT_READ,
                                MAP_PRIVATE + MAP_NOCORE, fd,
                                i * locp->memsize);
            if (locp->tmpmap == MAP_FAILED)
                LEAVE(locp, HXERR_MMAP);

            _store(locp, locp->tmpmap, partv[i].nrecs, &ovfl);
            DEBUG2("%d: %lld %d", i, partv[i].nbytes, partv[i].nrecs);

            munmap(locp->tmpmap, locp->mapsize);
            locp->tmpmap = 0;
        }

        DEBUG("split=%.3fs store=%.3fs", tick() - t3, t3 - t2);
    }

    // Populate bitmap

    ovfl -= HXPGRATE;
    if (ovfl && IS_MAP(hp, ovfl))
        ovfl -= HXPGRATE;
    int     bitpos;
    PAGENO  lastmap = _hxmap(hp, ovfl, &bitpos);

    _hxload(locp, bufp, 0);

    // Write completely-filled bitmap pages before the last having any set.
    memset(bufp->data + bufp->used, -1, DATASIZE(hp) - bufp->used);
    for (bufp->pgno = 0; bufp->pgno < lastmap;
         bufp->pgno = NEXT_MAP(hp, bufp->pgno)) {
        STAIN(bufp);
        _hxsave(locp, bufp);

        if (!bufp->pgno) {      // fixup after writing bufp as ROOT page.
            memset(bufp->data, -1, bufp->used);
            bufp->next = bufp->used = bufp->recs = bufp->orig = 0;
        }
    }

    // Write the last page having any bits set.
    int     len = bufp->used + bitpos / 8;

    if (len < (int)DATASIZE(hp)) {
        bufp->data[len] = ~(-2 << (bitpos & 7));
        memset(bufp->data + len + 1, 0, DATASIZE(hp) - len - 1);
    }
    STAIN(bufp);
    _hxsave(locp, bufp);

    // Remaining map pages are all zero except their self-allocation bit.
    bufp->next = bufp->used = bufp->recs = bufp->orig = 0;
    memset(bufp->data, 0, DATASIZE(hp));
    bufp->data[0] = 0x01;
    while ((bufp->pgno = NEXT_MAP(hp, bufp->pgno)) < locp->npages) {
        STAIN(bufp);
        _hxsave(locp, bufp);
    }

    // HOLD_FILE keeps other hx calls from locking/unlocking the file.
    HOLD_FILE(hp);
    if (hxdebug) DEBUG("after bulk load, hxcheck=%s", hxmode(hxfix(hp, 0,0,0,0)));

    // The hxfile is now consistent.
    // Use hxput to add 'overflow' records written to fp[1].
    // These may arise because of non-uniform partitioning,
    // and when _store() cannot fit the record in the file
    // using overflow pages already allocated.
    // _store is not allowed to change the HXFILE size!
    HXREC  *rp = (HXREC *) inpbuf;
    HXRET   rc;
    int     nputs = 0;

    rewind(locp->fp[1]);

    TICK(t4);
    while (fread(rp, sizeof(HXREC), 1, locp->fp[1])) {

        if (!(len = RECLENG(rp)) ||
            !fread((char *)(rp + 1), len, 1, locp->fp[1]))
            LEAVE(locp, HXERR_READ);
        if (0 > (rc = hxput(hp, RECDATA(rp), len)))
            LEAVE(locp, rc);
        if (hxdebug) {
            int save = hxdebug; hxdebug = 2;
            int chk = hxfix(hp,0,0,0,0);
            hxdebug = save;
            if (chk != HX_UPDATE)
                break;
        }
        ++nputs;
    }
    DEBUG("hxputs: %d %.3fs", nputs, tick() - t4);

    if (!feof(locp->fp[1]))
        LEAVE(locp, HXERR_READ);

    LEAVE(locp, locp->ret);
}

// Flush a partition's vbuf to disk. Never called unless
//  it is known that there is something to flush. If
//  nbytes (total data length in partition (i), including
//  vbuf) is a multiple of vbufsize, then the whole
//  vbuf needs to be flushed.
static void
_flush(HXLOCAL * locp, int vbufsize, int i, off_t nbytes)
{
    int     len = nbytes % vbufsize;

    if (!len)
        len = vbufsize;

    DEBUG2("part:%3d %3d len:%d %d", i, (int)(nbytes / vbufsize),
           len, nbytes % vbufsize);
    if (0 > lseek(fileno(locp->fp[2]), i * locp->memsize + nbytes - len, 0))
        LEAVE(locp, HXERR_LSEEK);

    if (0 > write(fileno(locp->fp[2]), locp->mem + i * vbufsize, len))
        LEAVE(locp, HXERR_WRITE);
}

static void
_parse(HXLOCAL * locp, char *inpbuf, int len, HXREC * rp, int recsize)
{
    if (inpbuf[--len] != '\n')
        LEAVE(locp, HXERR_BAD_RECORD);

    inpbuf[len] = 0;

    len = hx_load(locp->file, (char *)(rp + 1), recsize, inpbuf);

    if (len < 1 || len > recsize)
        LEAVE(locp, HXERR_BAD_RECORD);

    STLG(hx_hash(locp->file, RECDATA(rp)), &rp->hash);
    STSH(len, &rp->leng);
}

// _split first reads back and partitions locp->fp[0],
//  since that is almost certainly still in disk cache.
//  It then reads, parses and partitions the rest of (inpf).
static void
_split(HXLOCAL * locp, PART * partv, int nparts, int fd, FILE * inpf)
{
    HXFILE *hp = locp->file;
    int     middle = _hxd2f((locp->mask + 1) >> 1);
    int     split = SPLIT_PAGE(locp);
    int     i, len, d0 = 1;

    //vbufsize: must divide memsize evenly,
    //                and be a multiple of DISK_PGSIZE.
    int     vbufsize = locp->memsize / DISK_PGSIZE / nparts * DISK_PGSIZE;

    if (vbufsize < hxmaxrec(hp))    // memsize too small for inp
        LEAVE(locp, HXERR_BAD_REQUEST);

    DEBUG("parts:%d split:%d middle:%d vbuf:%dK",
          nparts, split, middle, vbufsize / 1024);

    memset(partv, 0, sizeof(PART) * nparts);

    if (ftruncate(fd, nparts * locp->memsize))
        LEAVE(locp, HXERR_FTRUNCATE);

    // Distribute records across partitions

    int const maxrec = DATASIZE(hp);
    char    recbuf[maxrec], inpbuf[maxrec * 3];
    HXREC  *rp = (HXREC *) recbuf;

    rewind(locp->fp[0]);
    while (1) {

        if (!(d0 &= fread(rp, sizeof *rp, 1, locp->fp[0])
              && (len = RECLENG(rp))
              && fread((char *)(rp + 1), len, 1, locp->fp[0]))) {

            if (!fgets(inpbuf, maxrec * 2, inpf))
                break;

            _parse(locp, inpbuf, strlen(inpbuf), rp, maxrec);
        }
        // Linear hash does not distribute equally: buckets in
        // the range [split..middle-1] get 2x as many records
        // as the rest. (i) is jinked to account for this.
        int     pg = _hxhead(locp, RECHASH(rp));

        i = (pg < split ? pg
             : pg < middle ? pg + pg - split : pg + middle - split)
            * nparts / (locp->npages + middle - split);

        len = RECSIZE(rp);
        if (partv[i].nbytes + len > locp->memsize) {

            if (!fwrite(rp, len, 1, locp->fp[1]))
                LEAVE(locp, HXERR_WRITE);

        } else {
            char   *src = (char *)rp;
            char   *vbuf = &locp->mem[i * vbufsize];
            int     pos = partv[i].nbytes % vbufsize;
            int     left = vbufsize - pos;

            if (len >= left) {
                memcpy(vbuf + pos, src, left);
                partv[i].nbytes += left;
                src += left;
                len -= left;
                _flush(locp, vbufsize, i, partv[i].nbytes);
                pos = 0;
            }

            memcpy(vbuf + pos, src, len);
            partv[i].nbytes += len;
            partv[i].nrecs++;
        }
    }

    if (!feof(locp->fp[0]))
        LEAVE(locp, HXERR_READ);

    for (i = 0; i < nparts; ++i)
        if (partv[i].nbytes % vbufsize)
            _flush(locp, vbufsize, i, partv[i].nbytes);
}

static void
_store(HXLOCAL * locp, char *cp, int nrecs, PAGENO * povfl)
{
    HXFILE *hp = locp->file;
    HXBUF  *bufp = &locp->buf[0];
    REC    *recv = locp->recv;
    int     i, j, orecs = 0, osize = 0;

    for (i = 0; i < nrecs; ++i, cp += RECSIZE(cp)) {
        assert((int)RECLENG(cp) <= hxmaxrec(hp));
        assert(hx_test(hp, RECDATA(cp), RECLENG(cp)));
        recv[i].recp = (HXREC *) cp;
        recv[i].head = _hxhead(locp, RECHASH(cp));
    }

    qsort(recv, nrecs, sizeof *recv, (cmpfn_t) cmprec);

    recv[nrecs].head = 0;       // stopper for "for(j=i..." loop

    for (i = 0; i < nrecs;) {
        _hxfresh(locp, bufp, recv[i].head);

        for (j = i; recv[++j].head == bufp->pgno;) {
        }
        int     s, t = j - 1, bytes = RECSIZE(recv[t].recp);

        // At this point, recs [i..j-1] all have the same head.
        // Within that range, [t..j-1] are guaranteed unique.
        // As each unique recv[s] is discovered, prepend it to
        // the unique range.
        for (s = t; --s >= i;) {
            if (recdiff(hp, recv[s].recp, recv[s + 1].recp)) {
                bytes += RECSIZE(recv[s].recp);
                recv[--t] = recv[s];
            }
        }

        PAGENO  minovfl = *povfl;

        for (i = t; i < j; ++i) {
            int     size = RECSIZE(recv[i].recp);

            if (!FITS(hp, bufp, size, 1)) {
                // Only use the (shared) tail page if the entire
                // remainder of this head's records will fit.
                if (hp->tail.pgno != bufp->pgno && hp->tail.pgno < minovfl
                    && FITS(hp, &hp->tail, bytes, j - i)) {
                    LINK(bufp, hp->tail.pgno);
                    _hxsave(locp, bufp);
                    _hxload(locp, bufp, bufp->next);
                } else if (*povfl < locp->npages) {

                    LINK(bufp, *povfl);
                    _hxsave(locp, bufp);
                    _hxfresh(locp, bufp, bufp->next);
                    *povfl += HXPGRATE;
                    if (IS_MAP(hp, *povfl))
                        *povfl += HXPGRATE;
                }
            }

            if (FITS(hp, bufp, size, 1)) {
                _hxappend(bufp, (char *)recv[i].recp, size);
                bufp->recs++;
            } else if (!fwrite(recv[i].recp, size, 1, locp->fp[1]))
                LEAVE(locp, HXERR_WRITE);
            else
                ++orecs, osize += size;

            bytes -= size;
        }

        _hxsave(locp, bufp);
    }

    if (orecs)
        DEBUG2("ovfl to temp: %d %d", orecs, osize);
}
