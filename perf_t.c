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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hx.h"
#include "hx_.h"
#include "util.h"

int
main(int argc, char **argv)
{
    char const *hx = getenv("hx");
    if (!hx) hx = ".";
    int     mapmode = HX_MMAP;
    char    inpdflt[strlen(hx) + sizeof "/88.tab"];
    char const *inpfile = inpdflt;
    int     memsize = 20;

    strcat(strcpy(inpdflt, hx), "/88.tab");

    if (argc == 1)
        usage("[-d] [inpfile [membits | 0]\n"
              "-d: use disk io, not mmap\n"
              "membits: size of RAM for hxbuild (default:20)\n"
              "  membits=0 means use the existing perf_t.hx\n");
    if (!strcmp(argv[1], "-d"))
        mapmode = 0, ++argv, --argc;

    switch (argc) {
    case 3:
        memsize = atoi(argv[2]);
    case 2:
        inpfile = argv[1];
    case 1:
        break;
    }

    HXFILE *hp;
    HXSTAT  info;
    HXRET   rc;
    FILE   *fp = strcmp(inpfile, "-") ? fopen(inpfile, "r") : stdin;

    if (!fp)
        die(": cannot read %s:", inpfile);
    setvbuf(fp, NULL, _IOFBF, 65536);
    if (memsize) {
        memsize = 1 << memsize;
        hxcreate("perf_t.hx", 0644, 4096, 0, 0);
        hp = hxopen("perf_t.hx", HX_UPDATE);

        double  tb = tick();

        rc = hxbuild(hp, fp, memsize, 0.0);
        if (rc < 0)
            die("hxbuild(%d): %s", memsize, hxerror(rc));
        fprintf(stderr, "build: %.4f\n", tick() - tb);

        hxclose(hp);
    }

    hp = hxopen("perf_t.hx", HX_UPDATE | mapmode);
    if (!hp)
        die("cannot open perf_t.hx%s:", mapmode ? " with MMAP" : "");

    char    buf[4096], rec[4096];

    double  t0 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, buf, sizeof buf);
    }

    double  t1 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        buf[0] ^= 0x55;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, buf, sizeof buf);
    }

    double  t2 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        strcpy(buf + strlen(buf) - 2, "hello, world");
        hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, buf, strlen(buf));
    }

    double  t3 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, buf, strlen(buf) - 11);
    }

    double  t4 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        char   *cp = buf + strlen(buf) - 2;

        memset(cp, '+', 100);
        strcpy(cp + 100, "\n");
        hxput(hp, buf, strlen(buf));
    }

    double  t5 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, buf, sizeof buf);
    }

    double  t6 = tick();

    rewind(fp);
    while (fgets(buf, sizeof buf, fp)) {
        buf[strlen(buf) - 1] = 0;
        buf[0] ^= 0x55;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, buf, sizeof buf);
    }

    double  t7 = tick();

    hxstat(hp, &info);
    fprintf(stderr,
            "nrecs:%g succ-get:%.4f fail-get:%.4f put+11:%.4f put-11:%.4f (usec) put+100:%.4f"
            " succ-get:%.4f fail-get:%.4f\n", info.nrecs,
            (t1 - t0) * 1E6 / info.nrecs, (t2 - t1) * 1E6 / info.nrecs,
            (t3 - t2) * 1E6 / info.nrecs, (t4 - t3) * 1E6 / info.nrecs,
            (t5 - t4) * 1E6 / info.nrecs, (t6 - t5) * 1E6 / info.nrecs,
            (t7 - t6) * 1E6 / info.nrecs);

    return 0;
}
