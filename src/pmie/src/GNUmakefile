#! gmake
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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#*************************************************************************
#
# Makefile - pmie
#
# This Makefile builds the pmie program executable.
#
#*************************************************************************
#
# $Id: GNUmakefile,v 2.39 2007/02/20 00:13:43 kimbrr Exp $
#

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

TARGET = pmie$(EXECSUFFIX)

CFILES	= pmie.c symbol.c dstruct.c lexicon.c syntax.c pragmatics.c eval.c \
       	  show.c match_inst.c syslog.c stomp.c andor.c

HFILES  = fun.h dstruct.h eval.h lexicon.h pmiestats.h pragmatics.h \
	  show.h symbol.h syntax.h syslog.h stomp.h andor.h

SKELETAL = hdr.sk fetch.sk misc.sk aggregate.sk unary.sk binary.sk \
	merge.sk act.sk

LSRCFILES = $(SKELETAL) meta syslog_solaris.h

LDIRT += $(YFILES:%.y=%.tab.?) fun.c fun.o $(TARGET) grammar.h

LLDLIBS = -lpcp -lm

default:	$(TARGET)

ifeq "$(TARGET_OS)" "interix"
# use these rules for platforms where yacc/bison is broken
CFILES	+= grammar.c
HFILES	+= grammar.h
grammar.c:	grammar.save.c
	cp grammar.save.c grammar.c
grammar.h:	grammar.save.h
	cp grammar.save.h grammar.h
LSRCFILES	+= grammar.y
LDIRT	+= grammar.c
else
# do it properly
YFILES = grammar.y
#Expect 184 shift/reduce conflict from bison
grammar.h:	grammar.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@
LSRCFILES	+= grammar.save.c grammar.save.h
endif

pmie$(EXECSUFFIX):	$(OBJECTS) fun.o
	$(CCF) -o $@ $(LDFLAGS) $(OBJECTS) fun.o $(LDLIBS)

install:	default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -m 644 pmiestats.h $(PCP_INC_DIR)/pmiestats.h

lexicon.o syntax.o:	grammar.h

fun.o:	fun.h

fun.c:	$(SKELETAL) meta
	@echo $@
	./meta

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
