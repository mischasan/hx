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

// "hxample" uses the default record-type library (hx_.so)
//  which implements (diff, hash, load, save, test).
// Compile it, then try:
//  ./hxample alpha beta gamma beta epsilon

#include <stdio.h>
#include <stdlib.h>             // putenv(3)
#include <string.h>
#include <unistd.h>             // access(2)
#include "hx.h"

int
main(int argc, char **argv)
{
    if (argc == 1)
        return fputs("Usage: hxample key ...\n", stderr);

    // Pagesize 4096 is realistic:
    int     rc = hxcreate("hxample.hx", /*perms */ 0664, /*pgsize */ 64,
                          /*rectype name */ "", /*strlen("") */ 0);

    printf("  hxcreate hxample.hx: %s\n", hxerror(rc));

    if (access("hx_.so", R_OK | X_OK))
        return fputs("'hx_.so' must be in this directory\n", stderr);
    char    ld_library_path[] = "LD_LIBRARY_PATH=.";

    putenv(ld_library_path);

    // HX_UPDATE + HX_MMAP for speed ...
    HXFILE *hp = hxopen("hxample.hx", HX_UPDATE);

    if (!hp)
        return fputs("Unable to open hxample.hx!?\n", stderr);

    char    buf[999], *val;
    int     i, len;

    // Insert (key,index)
    puts("# Insert keys with numeric values");
    for (i = 1; i < argc; ++i) {
        len = strlen(argv[i]);
        strcpy(buf, argv[i]);
        sprintf(val = buf + len + 1, "%08d", i);
        rc = hxput(hp, buf, len + 10);
        printf("  hxput(%s: %s) returned %d %s\n",
               buf, val, rc,
               rc < 0 ? hxerror(rc) : rc > 0 ? "replacement" : "");
    }

    puts("# Retrieve by key in reverse order");
    while (--i) {
        strcpy(buf, argv[i]);
        len = hxget(hp, buf, sizeof buf);
        val = buf + strlen(buf) + 1;    // Assumes the hxget succeeded.
        printf("  hxget(%s) returned %d\t%s(%s: %s)\n", argv[i],
               len, len < 0 ? hxerror(len) : "", buf, val);
    }

    puts("# Retrieve records in a hxnext loop");
    while ((len = hxnext(hp, buf, sizeof buf)) > 0) {
        val = buf + strlen(buf) + 1;
        printf("  hxnext returned %d\t%s(%s,%s)\n",
               len, len < 0 ? hxerror(len) : "", buf, val);
    }

    puts("# Append '-<key>' to each value");
    while (++i < argc) {
        strcpy(buf, argv[i]);
        // hxhold locks the record; superfluous here.
        len = hxhold(hp, buf, sizeof buf);
        printf("  hxhold returned %d\n", len);
        val = buf + strlen(buf) + 1;
        strcat(strcat(val, "-"), buf);
        len = hxput(hp, buf, len + 1 + strlen(buf));
        printf("  hxput(%s:%s) returned %d %s\n",
               buf, val, len, len < 0 ? hxerror(len) : "");
    }
    hxdebug = 3;
    puts("# Retrieve records after append, with hxdebug output");
    while ((len = hxnext(hp, buf, sizeof buf)) > 0) {
        val = buf + strlen(buf) + 1;
        printf("  hxnext returned %d\t%s(%s,%s)\n",
               len, len < 0 ? hxerror(len) : "", buf, val);
    }
    hxclose(hp);

    puts("# Now try some commands:\n"
         "  $ chx help\n"
         "  $ chx info hxample.hx\n"
         "  $ chx save hxample.hx\n" "  $ chx -v check hxample.hx\n");

    return 0;
}
