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
// Default HX (key,val) format:
//  Internal: "key\0val\0"
//  External: "key\tval\0"
#include <string.h>
#include "hx_.h"

int
diff(char const*ap, char const*bp, char const*udata, int uleng)
{
    (void)udata, (void)uleng;
    while (1) {
	if (*ap != *bp)
	    return 1;
	if (*ap == 0)
	    return 0;
	++ap, ++bp;
    }
}

HXHASH
hash(char const*recp, char const*udata, int uleng)
{
    (void)udata, (void)uleng;
    HXHASH  ret = 2166136261U;

    while (*recp && *recp != 0)
	ret = ret * 16777619U ^ *recp++;

    // For keys < 5bytes, scrambling the bits is reqd:
    ret += ret << 13;
    ret ^= ret >> 7;
    ret += ret << 3;
    ret ^= ret >> 17;
    ret += ret << 5;
    return ret;
}

int
load(char *recp, int recsize, char const *buf,
	char const*udata, int uleng)
{
    (void)udata, (void)uleng;
    // buf points to a string of the form "x+\tx*\0"
    int		buflen = strlen(buf) + 1;

    if (buflen <= recsize) {
	char *valpos = strchr(strcpy(recp, buf), '\t');
	if (valpos)
	    *valpos = 0;
    }

    return  buflen;
}

int
save(char const*recp, int reclen, char *buf, int bufsize,
	char const*udata, int uleng)
{
    (void)udata, (void)uleng;
    int	    keylen = strlen(recp) + 1;

    memcpy(buf, recp, reclen < bufsize ? reclen : bufsize);
    if (keylen <= bufsize)
	buf[keylen - 1] = '\t';
    buf[bufsize - 1] = 0;

    return  reclen;
}

int
test(char const*recp, int reclen, char const*udata, int uleng)
{
    (void)udata, (void)uleng;
    char    *cp = memchr(recp, 0, reclen);
    return  cp && !recp[reclen-1] 
            && reclen == 2 + (cp - recp) + (int)strlen(cp + 1) 
            && !memchr(recp, '\n', reclen);
}
