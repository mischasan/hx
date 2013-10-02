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
#include "_hx.h"
#include <ctype.h>
#include <fcntl.h>              // F_GETLK etc
#include <stdarg.h>
#include "util.h"

int     hxprwid = 55;
FILE   *hxlog;

// _hxcheckbuf: return HXOKAY if the page passes the page tests
//                  that hxcheck would apply:
//      MAP:  bad_{map_head map_self overmap}
//      DATA: bad_{next used rec_size rec_test rec_hash}
//  This routine is only for debugging and asserts.
//  Note: bad_dup_recs == 0 aka HXOKAY.

char   *
_hxblockstr(HXFILE * hp, char *outstr)
{
    struct flock what;

    what.l_type = F_WRLCK;
    what.l_start = 0;
    what.l_whence = SEEK_SET;
    what.l_len = 0;
    *outstr = 0;
    if (!fcntl(hp->fileno, F_GETLK, &what))
        sprintf(outstr, "pid:%d start:%d count:%d", what.l_pid,
                (PAGENO) ((int)what.l_start / hp->pgsize),
                (COUNT) (what.l_len / hp->pgsize));
    return outstr;
}

HXRET
_hxcheckbuf(HXLOCAL const *locp, HXBUF const *bufp)
{
    HXFILE *hp = locp->file;

    if (IS_MAP(hp, bufp->pgno)) {
        if (!(bufp->data[0] & 1))
            return bad_map_self;
        if (bufp->pgno) {       // extended map page.
            if (bufp->next || bufp->used)
                return bad_map_head;
        } else {                // root page.
            if (bufp->used >= DATASIZE(hp) || bufp->used != hp->uleng)
                return bad_map_head;
            // Todo: check that HXROOT{pgsize,version} are good.
        }

        int     lastbit, pos;
        PAGENO  lastmap = locp->npages - 1;

        lastmap = _hxmap(hp, lastmap - lastmap % HXPGRATE, &lastbit);
        if (bufp->pgno == lastmap) {    // All bits beyond lastbit must be zero.
            BYTE    mask = -2 << (lastbit & 7);

            for (pos = lastbit >> 3; pos < (int)DATASIZE(hp);
                 ++pos, mask = -1)
                if (bufp->data[pos] & mask)
                    return bad_overmap;
        }

    } else {                    // DATA page.
        if (bufp->next && bufp->next % HXPGRATE)
            return bad_next;
        if (bufp->used >= DATASIZE(hp))
            return bad_used;

        // If this is a tail (overflow) page, at least one
        // record must have the same _hxhead as locp->head.
        // Otherwise, EVERY record must match locp->head.
        int     recs = 0, is_tail = bufp->used && !bufp->next &&
            !IS_HEAD(bufp->pgno);
        char   *recp, *endp;
        PAGENO  head = IS_HEAD(bufp->pgno) ? bufp->pgno : locp->head;

        FOR_EACH_REC(recp, bufp, endp) {
            unsigned size = RECSIZE(recp);

            ++recs;
            if (size < MINRECSIZE || size > (unsigned)(endp - recp))
                return bad_rec_size;
            if (!hx_test(hp, RECDATA(recp), size - sizeof(HXREC)))
                return bad_rec_test;
            if (head && !is_tail &&
                head != _hxhead(locp, hx_hash(hp, RECDATA(recp)))) {
                DEBUG("bad head:%d",
                      _hxhead(locp, hx_hash(hp, RECDATA(recp))));
                return bad_rec_hash;
            }
        }

        if (recs != bufp->recs) {
            DEBUG("bad recs:%d", recs);
            return bad_recs;
        }

        if (recp != endp) {
            DEBUG("bad used:%ld", recp - bufp->data);
            return bad_recs;
        }
    }

    return HXOKAY;
}

void
_hxdebug(char const *func, int line, char const *fmt, ...)
{
    va_list ap;
    char   *msg, *cp;

    va_start(ap, fmt);
    int     len = vasprintf(&msg, fmt, ap);

    for (len = 0, cp = msg; *cp; ++cp, ++len)
        if (*cp == '\t')
            len |= 7;

    fprintf(hxlog ? hxlog : stderr,
            hxtime ? "%s%s%s:%d%s\t%.6f\n" : "%s%s%s:%d%s\n",
            msg, len < 64 ? "\t\t\t\t\t\t\t\t\t" + len / 8 : "\t",
            func, line, hxproc ? hxproc : "", hxtime ? tick() - hxtime : 0.0);
    free(msg);
}

char   *
_hxheads(HXLOCAL * locp, HXBUF const *bufp, char *outstr)
{
    HXFILE *hp = locp->file;

    *outstr = 0;
    if (IS_MAP(hp, bufp->pgno))
        return outstr;

    HXHASH  headv[MAX_RECS(hp) + 1], *vprev = locp->vprev;

    locp->vprev = headv;
    int     nheads = _hxfindHeads(locp, bufp);

    locp->vprev = vprev;

    char   *cp = outstr;

    while (nheads > 0)
        cp += sprintf(cp, "%d,", headv[--nheads]);
    if (cp > outstr)
        cp[-1] = 0;
    return outstr;
}

void
_hxprbuf(HXLOCAL const *locp, HXBUF const *bufp, FILE * fp)
{
    HXFILE *hp = locp->file;

    if (!fp) fp = hxlog ? hxlog : stderr;

    fprintf(fp, "pgno=%u", bufp->pgno);
    if (IS_MAP(hp, bufp->pgno)) {
        int     i, len;

        fprintf(fp, " MAP");
        if (!bufp->pgno) {
            HXROOT *rootp = (HXROOT *) bufp->page;
            char   *adata = acstr(bufp->data, bufp->used);

            fprintf(fp, " pgsize=%u udata[%u]=%s\n",
                    LDUS(&rootp->pgsize), bufp->used, adata);
            free(adata);
        }

        for (len = DATASIZE(hp); len > bufp->used && !bufp->data[len - 1];)
            --len;
        for (i = bufp->used; i < len; ++i)
            fprintf(fp, "%s%2.2X", i % 32 ? " " : "\n  ",
                    (unsigned char)bufp->data[i]);
        fputc('\n', fp);

    } else {
        int     strsize = hxmaxrec(hp) * 2;

        //int i; char sep = ':';
        char    str[strsize];
        char const *recp, *endp;

        fprintf(fp, " next=%u used=%u recs=%u orig=%d flag=%u\n"
                //" hsize=%d hmask=%d del(pos=%d len=%d) redex[%d]"
                , bufp->next, bufp->used, bufp->recs, bufp->orig, bufp->flag
                //, bufp->hsize, bufp->hmask, bufp->delpos, bufp->dellen, bufp->nredex
            );

        //for (i = 0; i < bufp->nredex; ++i, sep = ',') fprintf(fp, "%c%d", sep, bufp->redexv[i]); putc('\n', fp);

        FOR_EACH_REC(recp, bufp, endp) {
            HXHASH  h = RECHASH(recp);
            int     len = RECLENG(recp);
            char const *cp = RECDATA(recp);

            fprintf(fp, " %5" FOFF "d: %08X %5u [%d]",
                    recp - bufp->data, h, _hxhead(locp, h), len);
            if (hx_test(hp, cp, len)) {
                hx_save(hp, cp, len, str, strsize);
                fprintf(fp, "\t\"%.*s\"%s", hxprwid, str,
                        (int)strlen(str) > hxprwid ? "..." : "");
            } else if (len > 0) {
                dx(fp, cp, IMIN(len, 32));
            }

            putc('\n', fp);
        }

        if (!bufp->used)
            putc('\n', fp);

        //for (i = bufp->hsize; i-- > 0;) if (bufp->hind[-i]) fprintf(fp, " hind %3d @ %5d: %4d\n", -i, DATASIZE(hp) - 2*bufp->hsize, bufp->hind[-i]);
    }
}

void
_hxprfile(HXFILE const *hp)
{
    static char const *partname[] = { "NONE", "HEAD", "HIGH", "BOTH" };
    int save = hxdebug; hxdebug = 1;
    DEBUG("HXFILE tail:{pgno:%u used:%d recs:%d next:%d}"
          " hold:%d locked:%d lockv:[%d %d %d] lockpart:%s buffer:{...} head:%d mlen:%ld mmap:%p",
            hp->tail.pgno, hp->tail.used, hp->tail.recs,
            hp->tail.next, hp->hold, hp->locked, hp->lockv[0], hp->lockv[1], hp->lockv[2],
            partname[hp->lockpart], hp->head, hp->mlen, hp->mmap);
    hxdebug = save;
}

void
_hxprloc(HXLOCAL const *locp)
{
    int save = hxdebug; hxdebug = 1;
    DEBUG("HXLOCAL npages:%d dpages:%d split:%d mask:%d hash:%08X head:%d mode:%d mylock:%d changed:%d",
            locp->npages, locp->dpages, SPLIT_PAGE(locp), locp->mask, locp->hash, locp->head,
            locp->mode, locp->mylock, locp->changed);
    _hxprfile(locp->file);
    hxdebug = save;
}

void
_hxprlox(HXFILE * hp)
{
    int save = hxdebug; hxdebug = 1;
    static char who[99];
    DEBUG("blocked by %s", _hxblockstr(hp, who));
    hxdebug = save;
}
