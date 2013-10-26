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
// hx_badh: hx 'rectype' implementation without hash !
#include "hx_.h"

int
diff(char const *a __unused, char const *b __unused,
     char const *udata __unused, int uleng __unused)
{
    return 0;
}

int
load(char *recp __unused, int recsize __unused, char const *buf __unused,
     char const *udata __unused, int uleng __unused)
{
    return 0;
}

int
save(char const *recp __unused, int reclen __unused, char *buf __unused,
     int bufsize __unused, char const *udata __unused, int uleng __unused)
{
    return 0;
}

int
test(char const *recp __unused, int reclen __unused,
     char const *udata __unused, int uleng __unused)
{
    return 0;
}
