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
    int         mapmode = HX_MMAP;
    char        inpdflt[strlen(hx) + sizeof "/data.tab"];
    char const *inpfile = inpdflt;
    int         memsize = 0;
    strcat(strcpy(inpdflt, hx), "/data.tab");

    if (argc == 1) usage("[-d] inpfile [membits | 0]\n"
                         "   -d: use disk io, not mmap\n"
                         "   membits: size of RAM for hxbuild (default:20)\n"
                         "       membits=0 means use the existing perf_x.hx");
    if (!strcmp(argv[1], "-d"))
        mapmode = 0, ++argv, --argc;

    switch (argc) {
    case 3: memsize = atoi(argv[2]);
    case 2: inpfile = argv[1];
    case 1: break;
    }

    // Ensure that dirname(argv[0]) is in LD_LIBRARY_PATH:
    char *dir = strrchr(argv[0], '/'), empty[] = "";
    char const *llp = getenv("LD_LIBRARY_PATH");
    dir = dir ? (*dir = 0, argv[0]) : empty;
    if (!llp) llp = "";
    char llpath[strlen(dir) + strlen(llp) + 2];
    setenv("LD_LIBRARY_PATH", strcat(strcat(strcpy(llpath, dir), ":"), llp), /*OVERRIDE*/1);

    HXFILE  *hp;
    HXSTAT  info;
    HXRET   rc;

    FILE        *fp = strcmp(inpfile, "-") ? fopen(inpfile, "r") : stdin;
    if (!fp) die(": cannot read %s:", inpfile);
    setvbuf(fp, NULL, _IOFBF, 65536);

    if (memsize) {
        memsize = 1 << memsize;
        hxcreate("perf_x.hx", 0644, 4096, 0, 0);
        hp = hxopen("perf_x.hx", HX_UPDATE);
        rc = hxbuild(hp, fp, memsize, 0.0);
        if (rc < 0) die("hxbuild(%d): %s", memsize, hxerror(rc));
        hxclose(hp);
    }

    if (hxdebug) system("echo;echo built; chx info perf_x.hx; chx stat perf_x.hx; chx -vdd check perf_x.hx");
    hp = hxopen("perf_x.hx", HX_UPDATE | mapmode);
    if (!hp) die("cannot open perf_x.hx%s:", mapmode ? " with MMAP" : ""); 

    char    buf[4096], rec[4096];
    int     len;
    double t0 = tick();

    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, rec, sizeof(rec));
    }
    t0 = tick() - t0;

    double  t1 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        buf[0] ^= 0x55;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, rec, sizeof(rec));
    }
    t1 = tick() - t1;

    double  t2 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        strcat(buf, "hello, world");
        len = hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, rec, len);
    }
    t2 = tick() - t2;
    if (hxdebug) system("echo;echo put+12; chx info perf_x.hx; chx stat perf_x.hx; chx -vdd check perf_x.hx");

    double  t3 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        len = hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, rec, len);
    }
    t3 = tick() - t3;
    if (hxdebug) system("echo;echo put-0; chx info perf_x.hx; chx stat perf_x.hx; chx -vdd check perf_x.hx");

    double  t4 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        strcpy(buf + strlen(buf) - 1, "---------1---------2---------3---------4---------5---------6---------7---------8---------9---------0\n");
        len = hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, rec, len);
    }
    t4 = tick() - t4;
    if (hxdebug) system("echo;echo put+100; chx info perf_x.hx; chx stat perf_x.hx; chx -vdd check perf_x.hx");

    double  t5 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, rec, sizeof(rec));
    }
    t5 = tick() - t5;

    double  t6 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        buf[0] ^= 0x55;
        hx_load(hp, rec, sizeof rec, buf);
        hxget(hp, rec, sizeof(rec));
    }
    t6 = tick() - t6;

    double  t7 = tick();
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf) - 1] = 0;
        buf[0] ^= 0x55;
        hx_load(hp, rec, sizeof rec, buf);
        hxput(hp, rec, sizeof(rec));
    }
    t7 = tick() - t7;

    hxstat(hp, &info);
    fprintf(stderr, "nrecs: %g build: %.2fM rec/sec\n(usec:)\n"
                    "\tget-y\t%.2f\n\tget-n\t%.2f\n"
                    "\tput+12\t%.2f\n\tput+0\t%.2f\n\tput+100\t%.2f\n"
                    "\tget-y\t%.2f\n\tget-n\t%.2f\n\tput-xx\t%.2f\n",
            info.nrecs, info.nrecs/1E6/t0, t0*1E6/info.nrecs,
                        t1*1E6/info.nrecs, t2*1E6/info.nrecs,
                        t3*1E6/info.nrecs, t4*1E6/info.nrecs,
                        t5*1E6/info.nrecs, t6*1E6/info.nrecs,
                        t7*1E6/info.nrecs);

    return  0;
}
