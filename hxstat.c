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
// hxstat: show chain length distribution
//  and tail share distribution.

#include "_hx.h"

HXRET
hxstat(HXFILE * hp, HXSTAT * sp)
{
    HXLOCAL loc, *locp = &loc;
    PAGENO  pg;
    int     i, nchains;

    if (!hp || !sp)
        return HXERR_BAD_REQUEST;

    ENTER(locp, hp, 0, 1);
    HXBUF  *bufp = &locp->buf[0];

    locp->mode = F_RDLCK;
    _hxlock(locp, 0, 0);
    _hxsize(locp);

    memset(sp, 0, sizeof *sp);
    sp->npages = locp->npages;

    _hxinitRefs(locp);

    for (pg = 1; pg < (unsigned)locp->npages; ++pg) {

        _hxload(locp, bufp, pg);
        if (IS_MAP(hp, pg))
            continue;

        _hxsetRef(locp, pg, bufp->next);

        if (IS_HEAD(bufp->pgno)) {
            sp->head_bytes += bufp->used;
        } else if (bufp->used) {
            ++sp->ovfl_pages;
            sp->ovfl_bytes += bufp->used;
        }

        char   *recp, *endp;

        FOR_EACH_REC(recp, bufp, endp) {
            ++sp->nrecs;
            sp->hash ^= RECHASH(recp);
        }

        if (!IS_HEAD(pg)) {
            COUNT   nheads = 0;

            _hxfindHeads(locp, bufp);
            while (locp->vprev[nheads])
                ++nheads;
            if (nheads > HX_MAX_SHARE)
                nheads = HX_MAX_SHARE;
            ++sp->share_hist[nheads];
        }
    }

    for (pg = 1; pg < (unsigned)locp->npages; ++pg) {
        if (IS_HEAD(pg)) {
            unsigned j = pg, count = 0;

            while ((j = locp->vnext[j])
                   && count < HX_MAX_CHAIN + 1)
                ++count;

            ++sp->chain_hist[count];
        }
    }

    for (i = 0; i <= HX_MAX_CHAIN; ++i) {
        sp->avg_fail_pages += sp->chain_hist[i] * (i + 1);
        sp->avg_succ_pages += sp->chain_hist[i]
            * (int)((i + 2) / 2);
    }

    nchains = locp->npages * (HXPGRATE - 1) / HXPGRATE;
    sp->avg_fail_pages /= nchains;
    sp->avg_succ_pages /= nchains;

    LEAVE(locp, HXOKAY);
}
