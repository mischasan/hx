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
#include <errno.h>
#include "tap.h"
#include "_hx.h"

PAGENO  testv[][9] = {
    //NPAGES ---OUT---
    {5, 2, 1},
    {6, 2, 1},
    {7, 2},
    {8, 0},
    {9, 5, 3, 1},
    {10, 5, 1},
    {11, 1},
    {12, 0},
    {13, 5, 3, 2},
    {14, 5, 3},
    {15, 5},
    {16, 0},
    {17, 9, 7, 6},
    {18, 9, 7},
    {19, 9},
    {20, 0},
    {21, 10, 2, 1},
    {22, 2, 1},
    {23, 2},
    {27, 6},
    // steps over map1:,
    {29, 14, 13, 11, 10, 9, 7},
    {30, 14, 13, 11, 10, 9},
    {31, 14, 13, 11, 10},
    {33, 14, 13, 11},
    {34, 14, 13},
    {35, 14},
    {41, 21, 19, 1},
    {83, 41},
    {84, 0},
    {85, 42, 2, 1},
    {86, 2, 1}
};

int     ntests = sizeof testv / sizeof *testv;

int
main(int argc, char **argv)
{
    (void)argc;
    plan_tests(2 + ntests);

    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);

    //XXX: make leng=23, so that we test skipping (map1).
    int     rc = hxcreate("split_t.hx", 0644, 32, "ch\000", 23);

    ok(!rc, "created split_t.hx: %s", hxerror(rc));
    HXFILE *hp;

    ok(hp = hxopen("split_t.hx", 0), "opened split_t.hx");
    PAGENO  pgv[9];
    int     i, j;

    for (i = 0; i < ntests; ++i) {
        memset(pgv, 0, sizeof pgv);
        _hxsplits(hp, pgv, testv[i][0]);
        //for (j = 1; testv[i][j] && testv[i][j] == pgv[j-1]; ++j);
        for (j = 1; testv[i][j] == pgv[j - 1] && pgv[j - 1]; ++j);
        if (!ok(!testv[i][j], "split %u", testv[i][0])) {
            for (j = 0; pgv[j]; ++j)
                fprintf(stderr, " %u", pgv[j]);
            putc('\n', stderr);
        }
    }

    double  sum = 0;

    for (i = 4; i < 1000; ++i) {
        memset(pgv, 0, sizeof pgv);
        _hxsplits(hp, pgv, i);
        sum += (double)pgv[0] / i;
    }
    diag("waste: %.2f", sum / 1000);

    while (*++argv) {
        memset(pgv, 0, sizeof pgv);
        _hxsplits(hp, pgv, atoi(*argv));
        printf("%7s:", *argv);
        for (j = 0; pgv[j]; ++j)
            printf(" %u", pgv[j]);
        putchar('\n');
    }
    hxclose(hp);
    unlink("split_t.hx");

    return exit_status();
}
