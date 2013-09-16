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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tap.h"

#include "_hx.h"
#include "util.h"

#define NRECS       100
#define NITERS      50
static void     progress(HXFILE *hp, char who);
static int      putbuf(HXFILE *hp, char *buf, int key, int len);

static double   last;

int
main(int argc, char **argv)
{
    plan_tests(4);

    int     nrecs = argc > 1 ? atoi(argv[1]) : 100;
    int     niters = argc > 2 ? atoi(argv[2]) : 50;

    int     rc = hxcreate("conc_t.hx", 0755, 4096, 0, 0);
    ok(rc == 0, "created conc_t.hx: %s", hxerror(rc));

    // Load hxfile with records
#   define  _ "....:...."
    char const  val[] = "k.......\0"_"1"_"2"_"3"_"4"_"5"_"6"_"7"_"8"_"9"_"0"_"A"_"B"_"C"_"D"_"E";
    char        buf[300];
#   undef   _
    memcpy(buf, val, sizeof(val));

    HXFILE      *hp = hxopen("conc_t.hx", HX_UPDATE);
    if (!hp) {
        perror("cannot open conc_t.hx");
        return 1;
    }

    int         i, j, nops = 0;
    for (i = 0; !rc && i < nrecs; ++i) {
        if (i % 100 == 0) putc('.', stderr);
        rc = putbuf(hp, buf, i, 10 + i % 100);
    }
    putc('\n', stderr);

    hxclose(hp);
    ok(rc == 0, "%d inserts: %s", i, hxerror(rc));

    system("unset HXDEBUG; set -x; if chx -v check conc_t.hx; then chx info conc_t.hx; chx stat conc_t.hx; fi >&2");
    char    who[99]; hxproc = who;
    if (!fork()) {
        strcpy(who, " {del}");
        hp = hxopen("conc_t.hx", HX_UPDATE);
        last = tick();
        for (i = 0; rc >= 0 && i < nrecs * niters - 1; ++i) {
            if (!(i % nrecs)) progress(hp, '-');
            // "617": closest prime to 618, which is close to phi.
            sprintf(buf+2, "%06d", i * 617 % nrecs);
            sprintf(who, " {del %d}", i * 617 % nrecs);
            if (hxdebug) fprintf(stderr, "    hxdel(%s)...\n", buf);
            rc = hxdel(hp, buf);
            if (hxdebug) fprintf(stderr, "    hxdel(%s): %d\n", buf, rc);
            if (rc > 0) ++nops;
        }
        if (rc < 0) fprintf(stderr, "failed on hxdel(%s)\n", buf);
        fprintf(stderr, "\tndels: %d\n", nops);
        exit(rc > 0 ? 0 : -rc);
    }

    strcpy(who, " {put}");
    hp = hxopen("conc_t.hx", HX_UPDATE);
    last = tick();
    for (j = 0; j < niters; ++j) {
        for (i = 0; rc >= 0 && i < nrecs; ++i) {
            if (!(i % nrecs)) progress(hp, '+');
            sprintf(who, " {put %d}", i);
            rc = putbuf(hp, buf, i, 15 + (j + nrecs + niters - i) % 100);
            if (rc == 0) ++nops;
        }
        if (rc < 0) { fprintf(stderr, "failed on hxput(%s): %s\n", buf, hxerror(rc)); break; }
    }

    wait(&rc);
    fprintf(stderr, "\tnadds: %d\n", nops);
    ok(rc >= 0, "%d/%d updates: %s", i, nrecs*niters, hxerror(rc));
    ok(!rc, "deleter succeeded: %s", hxerror(-rc >> 8));
    system("unset HXDEBUG; set -x; if chx -dv check conc_t.hx; then chx info conc_t.hx; chx stat conc_t.hx; fi >&2");

    return exit_status();
}

static void
progress(HXFILE *hp, char who)
{
    (void)hp, (void)who;
#if 0
    (void)hp; putc(who, stderr);
    HXSTAT      s;
    double      now = tick();
    hxstat(hp, &s);
    fprintf(stderr, "%c %6.0f %08X %5.2f\n", who, s.nrecs, s.hash, now - last);
    last = tick();
#endif
}

static int
putbuf(HXFILE *hp, char *buf, int key, int len)
{
    sprintf(buf+2, "%06d", key);
    char    ch = buf[len - 1];
    buf[len - 1] = 0;
    int     rc = hxput(hp, buf, len);
    buf[len - 1] = ch;
    return  rc;
}
