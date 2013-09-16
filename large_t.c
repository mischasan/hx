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
#include "tap.h"
#include <errno.h>
#include <sys/stat.h>
#include "_hx.h"
#include "hx_.h"
#include "util.h"

int
main(int argc, char **argv)
{
    int     count = argc > 1 ? atoi(argv[1]) : 10000;
    plan_tests(9);

    ok(sizeof(off_t) == 8, "sizeof(off_t) == %"FSIZE"d", sizeof(off_t));

    // TODO: add tests that actually test hx() on large databases
    int     npages  = 1 << 20;
    int     pgsize  = 8192;
    int     rc = hxcreate("large_t.hx", 0666, pgsize, NULL, 0);
    ok(rc == HXOKAY, "hxcreate(large_t.hx): %d %s", rc, hxerror(rc));

    HXFILE  *hp = hxopen("large_t.hx", HX_UPDATE);
    ok(hp != NULL, "hxopen(large_t.hx): %d %s", errno, errname[errno]);
    int     fd = hxfileno(hp);
    off_t   size = (off_t)pgsize * npages;   // 8GB
    ok(!ftruncate(fd, size), "grow to %"FOFF"d: %d %s", size, errno, errname[errno]);
    // Manually initialize map pages. 
    // A map page must indicate that it itself (i.e. bit 0 in the map) is
    // allocated, else it will eventually be allocated and overwritten as
    // an overflow page.
    // 32:8(bits/byte)*4(pgrate) pgsize:bytes/page  6:(page header overhead)
    int     pg, last = _hxmap(hp, npages, &pg); // "pg" is just junk here.
    char    map = 1;
    for (pg = 0; (pg = NEXT_MAP(hp, pg)) <= last;) {
        lseek(fd, (off_t)pg * pgsize + sizeof(HXPAGE), 0);
        write(fd, &map, 1);
    }

    struct stat sb;
    ok(!fstat(hxfileno(hp), &sb), "fstat: %d %s", errno, errname[errno]);
    ok(sb.st_size == size, "size is 0x%"FOFF"x", sb.st_size);

    // 10000 records is enough to ensure that at least SOME records end up 
    // above the 4GB mark.
    int         i, tally = 0;
    char        rec[11] = {};
    for (i = rc = 0; i < count; ++i) {
        sprintf(rec, "%08d", i);
        rc = hxput(hp, rec, 10);
        if (rc != 0) break;
        rc = hxget(hp, rec, 10);
        if (rc != 10) { fprintf(stderr, "hxget(%s) failed: %d\n", rec, rc); break; }
    }
    ok(i == count, "inserted and retrieved %d/%d records: %s", i, count, hxerror(rc));

    while (0 < (rc = hxnext(hp, rec, 11))) ++tally;
    ok(tally == count, "hxnext retrieved %d/%d records: %s", tally, count, hxerror(rc));

    rc = hxfix(hp,0,0,0,0);
    ok(rc == (HXRET)HX_UPDATE, "hxcheck: large_t.hx is ready for %s access", hxmode(rc));

    hxclose(hp);

    return exit_status();
}
