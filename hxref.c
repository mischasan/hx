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
// hxref: manage in-memory xref of all pages' (next) fields.
//  Used by hxfix and hxpack.

#include <assert.h>

#include "_hx.h"

//--------------|---------------------------------------------
void
_hxinitRefs(HXLOCAL * locp)
{
    int     novers = (locp->npages - 1) / HXPGRATE + 1;
    int     maxrefs = DATASIZE(locp->file) / MINRECSIZE;

    locp->vnext = calloc(locp->npages, sizeof(PAGENO));
    locp->vprev = calloc(maxrefs, sizeof(PAGENO));
    locp->vrefs = calloc(novers, sizeof(COUNT));
}

void
_hxsetRef(HXLOCAL * locp, PAGENO pg, PAGENO next)
{
    --VREF(locp, locp->vnext[pg]);
    ++VREF(locp, next);
    assert(!locp->vnext[pg] || locp->vnext[pg] != next);
    locp->vnext[pg] = next;
}

PAGENO
_hxgetRef(HXLOCAL const *locp, PAGENO pg, PAGENO tail)
{
    PAGENO  ret = pg;

    while (locp->vnext[ret] != tail) {
        ret = locp->vnext[ret];
        assert(ret);
    }

    return ret;
}

int
_hxfindHeads(HXLOCAL const *locp, HXBUF const *bufp)
{
    PAGENO  head, *pp, *zprev = locp->vprev;
    char   *recp, *endp;

    FOR_EACH_REC(recp, bufp, endp) {
        head = _hxhead(locp, RECHASH(recp));
        for (pp = locp->vprev; pp < zprev && *pp != head; ++pp);
        if (pp == zprev)
            *zprev++ = head;
    }

    *zprev = 0;
    return zprev - locp->vprev;
}

// _hxfindRefs: fill vprev[] with a (0)-terminated list of pgnos
//  whose (next) links are (pg), for every chain that some
//  record in (bufp) hashes to.
void
_hxfindRefs(HXLOCAL * locp, HXBUF const *bufp, PAGENO pg)
{
    PAGENO *pp;

    _hxfindHeads(locp, bufp);
    for (pp = locp->vprev; *pp; ++pp) {
        *pp = _hxgetRef(locp, *pp, pg);
    }
}

// EOF
