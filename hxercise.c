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

// hxercise: test a hx_ rectype that implements all five functions
#include <stdarg.h>

#include "_hx.h"

void hxercise(HXFILE *hp, char const *str, FILE *fp);
static int _ok(FILE *fp, int pass, char const*fmt, ...);

static int memspan(char const *buf, char ch, int len)
{
    while (len-- >= 0 && buf[len] == ch);
    return  len < 0;
}

static char fill = '#';

void
hxercise(HXFILE *hp, char const *str, FILE *fp)
{
    int     maxrec = hxmaxrec(hp), reclim = maxrec * 2, buflim = reclim;
    char    *recspc = malloc(reclim), *rec = recspc + 5;
    char    *bufspc = malloc(buflim), *buf = bufspc + 7;
    int     i, test[2] = {};

#   define  OK(...) ++test[_ok(fp, __VA_ARGS__)]

    memset(recspc, fill, reclim);
    int     len1 = hx_load(hp, rec, 0, str);
    OK(len1 > 0 && len1 <= maxrec, "hx_load(,,0,) returns: %d in 1..%d for '%s'", len1, maxrec, str);
    OK(memspan(recspc, fill, reclim), "hx_load(,rec,0,) leaves rec unchanged");

    int     len2 = hx_load(hp, rec, len1, str);
    OK(len2 == len1 && memspan(recspc, fill, 5)
        && memspan(rec+len1, fill, reclim-5-len1), "hx_load(,,%d,): %d", len1, len2);

    char    rec2[len1];
    memcpy(rec2, rec, len1);
    OK(!hx_diff(hp, rec2, rec), "diff of copy");
    OK(hx_hash(hp, rec2) == hx_hash(hp, rec), "hash of copy");

    memset(recspc, fill, reclim);
    for (i = 1; i < len1; ++i) {
        len2 = hx_load(hp, rec, i, str);
        OK(len2 == len1 && memspan(recspc, fill, 5) && !memcmp(rec, buf, i) 
            && memspan(rec+i, fill, reclim-5-i), "hx_load(,,%d,): %d", i ,len2); 
    }

    memcpy(buf, rec, len1);
    for (i = len1; i > 0; --i) {
        if (hx_test(hp, buf, i)) {
            buf[i] ^= 0x5A;
            OK(hx_test(hp, buf, i), "retest %d", i);
        }
    }

    memset(bufspc, fill, buflim);
    len2 = hx_save(hp, rec2, len1, buf, 0);
    OK(memspan(bufspc, fill, buflim), "hx_save(,,,,0) leaves target unchanged");

    if (len2 > buflim - 5) {
        buflim *= 2;
        bufspc = realloc(bufspc, buflim);
        memset(bufspc, fill, buflim);
        buf = bufspc + 7;
    }

    int     len3 = hx_save(hp, rec2, len1, buf, len2 + 1);
    OK(len3 == len2 && memspan(buf, fill, 7) && memspan(buf, fill, buflim-7-len2)
        && buf[len3-1] && (int)strlen(buf) == len3-1 && strchr(buf, '\t') 
        && !strchr(buf, '\n'), "hx_save(,,%d,,%d): %d", len1, len2+1, len3);
     
    DEBUG2("+%d -%d: '%s'", test[0], test[1], str);
    free(recspc);
    free(bufspc);
}

static int
_ok(FILE *fp, int pass, char const*fmt, ...)
{
    va_list	ap;

    if (fp) {
        fputs(pass ? "ok" : "not ok", fp);
        char	*msg;

        va_start(ap, fmt);
        vasprintf(&msg, fmt, ap);
        if (*msg) fprintf(fp, " - %s\n", msg);
        free(msg);
    }

    return  pass;
}
