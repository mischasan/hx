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
#include <string.h>
#include "hx_.h"

static inline int
IMIN(int a, int b)
{
    return a < b ? a : b;
}

int
diff(char const *a, char const *b,
     char const *udata __unused, int uleng __unused)
{
    return *(short const *)a - *(short const *)b;
}

HXHASH
hash(char const *recp, char const *udata __unused, int uleng __unused)
{
    return *recp;
}

int
load(char *recp, int recsize, char const *buf,
     char const *udata __unused, int uleng __unused)
{
    int     buflen = strlen(buf);

    memcpy(recp, buf, IMIN(buflen, recsize));
    return buflen;
}

int
save(char const *recp, int reclen, char *buf, int bufsize,
     char const *udata __unused, int uleng __unused)
{
    if (bufsize > 0) {
        int     len = IMIN(reclen, bufsize - 1);

        memcpy(buf, recp, len);
        buf[len] = 0;
    }

    return reclen + 1;
}

int
test(char const *recp, int reclen,
     char const *udata __unused, int uleng __unused)
{
    if (reclen < 2)
        return 0;

    for (; --reclen >= 0; ++recp)
        if (*recp < ' ' || *recp > '~')
            return 0;

    return 1;
}
