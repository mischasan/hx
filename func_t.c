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

int
main(void)
{
    setenv("hx", ".", 0);
    setvbuf(stdout, 0, _IOLBF, 0);

    struct { PAGENO inp, exp; } try_split[] = {
        {2,1}, {3,1}, {4,2}, {5,1}, {6,2}, {7,3}
    };
    int i, ntry_split = sizeof try_split/sizeof*try_split;

    plan_tests(ntry_split);

    for (i = 0; i < ntry_split; ++i) {
        PAGENO act = _hxf2d(SPLIT_LO(_hxd2f(try_split[i].inp)));
        is(act, try_split[i].exp, "SPLIT_LO(%d)", try_split[i].inp);
    }

    return exit_status();
}
