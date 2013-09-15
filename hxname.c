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
#include "hx.h"

#define  NELS(v) (int)(sizeof(v) / sizeof(v[0]))
char const*
hxerror(HXRET ret)
{
#   undef   _C
#   undef   _E
#   define  _C	    ,
#   define  _E(x)   #x
    static char const *errv[] = { "ok", HXERRS };
    static char num[11]; 
    if (ret > 0 || -ret >= NELS(errv))
	return (sprintf(num, "%d", ret), num);
    return  errv[-ret];
}

char const*
hxmode(int mode)
{
    static char const *modev[] = {
	"READ",
	"UPDATE",
	"CHECK",
	"REPAIR",
	"READ,MMAP",
	"UPDATE,MMAP",
	"INVALID:CHECK+MMAP",
	"INVALID:REPAIR+MMAP",
	"DUBIOUS:READ+MPROTECT",
	"DUBIOUS:UPDATE+MPROTECT",
	"INVALID:CHECK+MPROTECT",
	"INVALID:REPAIR+MPROTECT",
	"READ+MPROTECT",
	"UPDATE+MPROTECT",
	"INVALID:CHECK+MMAP+MPROTECT",
	"INVALID:REPAIR+MMAP+MPROTECT",
	"DUBIOUS:READ+FSYNC",
	"UPDATE,FSYNC",
	"DUBIOUS:CHECK+FSYNC",
	"REPAIR,FSYNC",
    };

    return  mode < 0		? hxerror(mode) :
	    mode < NELS(modev)	? modev[mode]	: "INVALID";
}
