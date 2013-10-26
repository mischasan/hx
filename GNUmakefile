# Copyright (C) 2001-2013 Mischa Sandberg <mischasan@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License Version 2 as
# published by the Free Software Foundation.  You may not use, modify or
# distribute this program under any other version of the GNU General
# Public License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
#  IF YOU HAVE NO WAY OF WORKING WITH GPL, CONTACT ME.
#-------------------------------------------------------------------------------

-include rules.mk

#---------------- GLOBAL VARS:
# Used by some test programs:
export hx       ?= .

#---------------- PRIVATE VARS:
hx.o            =  $(patsubst %, $(hx)/%, hx.o hxbuild.o hxcheck.o hxcreate.o hxdiag.o hxget.o hxlox.o hxname.o hxnext.o hxopen.o hxput.o hxref.o hxshape.o hxstat.o hxupd.o util.o)
hx.rec          =  $(patsubst %, $(hx)/%, hx_ch.so hx_badb.so hx_badd.so hx_badh.so)
hx.tpgm         := $(patsubst %, $(hx)/%, hxample perf_x basic_t check_t conc_t corrupt_t func_t large_t lock_t many_t next_t build_t)

#---------------- PUBLIC VARS: inputs to install/clean/cover/test
# Currently $(all) is only used by "clean:" to magically delete cov/prof output files.

hx.bin          = $(hx)/chx
hx.include      = $(hx)/hx.h $(hx)/hx_.h
hx.lib          = $(hx)/libhx.a $(hx)/hx_.so

# Magic global variables ... note that unit-test tempfiles are always in cwd.
all		+= hx
clean 		+= $(hx.tpgm:%=%*.hx) bad_t.hx many_t.log.*

hx.cover        = $(hx.o:.o=.c)
hx.test         = $(patsubst %, %.pass, $(filter %_t,$(hx.tpgm)))

#---------------- PUBLIC RULES:
all     .PHONY  : hx.all
test    .PHONY  : hx.test
cover   .PHONY  : hx.cover
profile .PHONY  : hx.profile
#GMAKE does not apply patterns (e.g. %.install:...) to .PHONY targets
install         : hx.install

#---------------- PRIVATE RULES:
hx.all          : $(hx.bin) $(hx.lib)
hx.test         : hx.all $(hx.test)

$(hx.bin)       : $(hx)/libhx.a
$(hx)/libhx.a   : $(hx.o)

# tap requires -pthread:
$(hx.tpgm)      : LDLIBS += -pthread
$(hx.tpgm)      : $(hx)/hx_.so $(hx)/libhx.a $(hx)/tap.o

$(hx.test)      : LD_LIBRARY_PATH := $(hx):$(LD_LIBRARY_PATH)
$(hx.test)      : PATH := $(hx):$(PATH)
$(hx.test)      : $(hx)/hx_.so $(hx)/hx_ch.so

# basic_t uses ALL hx record-type .so's
$(hx)/basic_t.pass : $(hx.rec)
$(hx)/build_t.pass : $(hx)/data.tab

# Some test programs call system("chx ...");
$(hx)/basic_t.pass $(hx)/build_t.pass $(hx)/conc_t.pass $(hx)/many_t.pass  : $(hx)/chx

$(hx.o:%.o=%.[Isio])  : CPPFLAGS += -I$(hx) -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
$(hx.tpgm:%=%.[Isio]) : CPPFLAGS := -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 $(filter-out -D_FORTIFY_SOURCE%, $(CPPFLAGS))

-include $(hx)/*.d
# vim: set nowrap :
