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

// This is a subset of util/msutil.h
#ifndef UTIL_H
#define UTIL_H

// Convenient format strings for intptr_t etc
//  The strings do NOT include the "%", so that the user can specify
//  width/justification etc.
#ifdef __x86_64__
#   define FPTR     "l"
#elif __SIZEOF_POINTER__ == __SIZEOF_INT__
#   define FPTR
#elif __SIZEOF_POINTER__ == __SIZEOF_LONG__
#   define FPTR     "l"
#else
#   error need to define FPTR
#endif

// This does not work on gcc 4.1 (64)
#if __LONG_LONG_MAX__ == __LONG_MAX__
#   define FSIZE    "l"
#elif __SIZEOF_SIZE_T__ == __SIZEOF_INT__
#   define FSIZE
#elif __SIZEOF_SIZE_T__ == __SIZEOF_LONG__
#   define FSIZE    "l"
#else
#   error need to define FSIZE
#endif

#if __LONG_MAX__ == 9223372036854775807L
#   define F64      "l"
#elif __LONG_MAX__ == 2147483647L
#   define F64      "ll"
#else
#   error need to defined F64
#endif

#define FOFF FPTR

extern char const *errname[];

char*       acstr(char const *buf, int len);
void        die(char const *fmt, ...);
void        dx(FILE *fp, char const *buf, int len);

#if defined(__linux__)
char const *getprogname(void);  // BSD-equivalent
#endif

int         systemf(char const *fmt, ...);
double      tick(void);
void        usage(char const *str);

#endif//UTIL_H
