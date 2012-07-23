#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#*************************************************************************
#
# Makefile - pmie
#
# This Makefile builds the pmie program executable.
#
#*************************************************************************

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

TARGET = pmie$(EXECSUFFIX)

CFILES	= pmie.c symbol.c dstruct.c lexicon.c syntax.c pragmatics.c eval.c \
	  show.c match_inst.c syslog.c stomp.c andor.c

HFILES  = fun.h dstruct.h eval.h lexicon.h pragmatics.h \
	  show.h symbol.h syntax.h syslog.h stomp.h andor.h

SKELETAL = hdr.sk fetch.sk misc.sk aggregate.sk unary.sk binary.sk \
	merge.sk act.sk

LSRCFILES = $(SKELETAL) meta logger.h

LDIRT += $(YFILES:%.y=%.tab.?) fun.c fun.o $(TARGET) grammar.h

LLDLIBS = -lpcp -lm $(LIB_FOR_REGEX)

LCFLAGS = $(PIECFLAGS)
LLDFLAGS = $(PIELDFLAGS)

default:	$(TARGET)

YFILES = grammar.y
grammar.h grammar.tab.c:	grammar.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@

pmie$(EXECSUFFIX):	$(OBJECTS) fun.o
	$(CCF) -o $@ $(LDFLAGS) $(OBJECTS) fun.o $(LDLIBS)

install:	default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)

lexicon.o syntax.o:	grammar.h

fun.o:	fun.h

fun.c:	$(SKELETAL) meta
	@echo $@
	./meta

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
