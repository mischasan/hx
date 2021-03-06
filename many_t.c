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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tap.h"

#include "hx.h"
#include "util.h"

void    _hxdebug(char const *func, int line, char const *fmt, ...);

int
main(int argc, char **argv)
{
    int     nprocs = argc > 1 ? atoi(argv[1]) : 20;
    int     nrecs = argc > 2 ? atoi(argv[2]) : 10000;
    int     i, j, pid, status;
    if (nprocs < 2) die(": procs must be > 1");
    nrecs /= nprocs;

    system("rm -rf many_t.log.*");
    setenv("hx", ".", 0);

    plan_tests(4);

    int     rc = hxcreate("many_t.hx", 0755, 4096, 0, 0);
    ok(rc == 0, "created many_t.hx: %s", hxerror(rc));

    char    buf[200];
    double  t0 = hxtime = tick();

    for (i = 0; i < nprocs; ++i) {
        if ((pid = fork()))
            continue;

        // A HXFILE without HX_MMAP cannot be shared across fork().
        HXFILE *hp = hxopen("many_t.hx", HX_UPDATE + HX_MMAP);

        char    logname[20];
        sprintf(logname, "many_t.log.%05d", getpid());
        if (hxdebug)
            setvbuf(stderr = hxlog = fopen(logname, "w"), NULL, _IOLBF, 0);
        logname[10] = '\t';
        hxproc = logname + 10;

        memset(buf, '-', sizeof buf);
        memcpy(buf, logname + 11, 5);   // pid

        pid = getpid();
        for (j = 0; j < nrecs; ++j) {
            sprintf(buf + 6, "%06d", j);    // recnum + \0 (key terminator)
            buf[13] = '[';
            int     len = (j % 11) * 11 + 16;

            if (hxdebug > 1)
                fprintf(hxlog, "@PUT %s %d\t%05d\t%.6f\n", buf, len, pid, tick() - hxtime);

            buf[len - 1] = ']', buf[len] = 0;
                // buf: "PID-RECNUM\0[--- ... ---]\0"
            rc = hxput(hp, buf, len + 1);
            buf[len - 1] = '-', buf[len] = '-';
            if (rc) {
                fprintf(stderr, "%s at %s\n", hxerror(rc), buf);
                break;
            }
        }
        exit(i + 1);
    }

    while (-1 != (pid = wait(&status)))
        if (hxdebug)
            fprintf(stderr, "wait: pid:%d status:%x\n", pid, status);

    printf("procs: %4d recs: %6d elapsed: %.4f\n", nprocs, nrecs, tick() - t0);

    HXFILE *hp = hxopen("many_t.hx", HX_UPDATE);

    hxdebug = 0;
    for (i = 0; 0 < (rc = hxnext(hp, buf, sizeof buf)); ++i);
    ok(rc == HXOKAY, "hxnext scans file: %s", hxerror(rc));
    ok(i == nrecs*nprocs, "hxnext finds %d/%d recs", i, nrecs*nprocs);
    rc = hxfix(hp, NULL, 0, 0, 0);
    hxclose(hp);
    ok(rc == HX_UPDATE, "hxcheck returns: %s", hxmode(rc));
    if (rc != HX_UPDATE)
        system("(HXDEBUG=; set -x; $hx/chx check -dd many_t.hx; $hx/chx info many_t.hx; $hx/chx stat many_t.hx) >&2");

    return exit_status();
}
