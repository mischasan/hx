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

#include "_hx.h"
#include "util.h"

static void try(char const *, int nrecs);

int
main(void)
{
    char    input[999];
    char const *envhx = getenv("hx");

    if (!envhx)
        envhx = ".";

    setvbuf(stdout, 0, _IOLBF, 0);

    plan_tests(1 + 4 * 4);

    HXRET   rc = hxbuild(0, 0, 1 << 20, 0.0);

    ok(rc == HXERR_BAD_REQUEST, "hxbuild rejects NULL arg: %s", hxerror(rc));

    try("/dev/null", 0);
    try(strcat(strcpy(input, envhx), "/data.tab"), 88172);
    try(strcat(strcat(strcpy(input, "cat "), envhx), "/data.tab"), 88172);
    try(strcat(strcat(strcpy(input, "head -2335 "), envhx), "/data.tab"),
        2335);

    return exit_status();
}

static void
try(char const *inpfile, int nrecs)
{
    FILE   *fp =
        strchr(inpfile, ' ') ? popen(inpfile, "r") : fopen(inpfile, "r");
    ok(fp, "input %s", inpfile);
    if (!fp)
        die(": unable to open %s:", inpfile);
    setvbuf(fp, NULL, _IOFBF, 16384);

    hxcreate("build_t.hx", 0755, 4096, "", 0);

    HXFILE *hp = hxopen("build_t.hx", HX_UPDATE);

    double  alpha = tick();
    HXRET   rc = hxbuild(hp, fp, 1 << 20, 0.0); //128MB
    double  omega = tick();

    if (strchr(inpfile, ' '))
        pclose(fp);
    else
        fclose(fp);

    ok(rc == 0, "hxbuild from (%s) in %.3g secs, returns %s", inpfile,
       omega - alpha, hxerror(rc));

    rc = hxfix(hp, 0, 0, 0, 0);
    ok(rc == (HXRET) HX_UPDATE, "hxcheck returns: %s", hxmode(rc));

    HXSTAT  st;

    hxstat(hp, &st);
    ok(st.nrecs == nrecs, "hxbuild loaded %.0f/%d records", st.nrecs, nrecs);
    hxclose(hp);
    unlink("build_t.hx");
}
