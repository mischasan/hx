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

// hx_.h: interface definitions for record-type library implementers.

#ifndef HX__H
#define HX__H
#include "hx.h"

int     diff(char const *, char const *, char const *, int);
HXHASH  hash(char const *, char const *, int);
int     load(char *, int recsize, char const *, char const *, int);
int     save(char const *, int reclen, char *, int bufsize, char const *, int);
int     test(char const *, int reclen, char const *, int);

#ifndef __unused
#   define __unused __attribute__((unused))
#endif
#endif //HX__H
