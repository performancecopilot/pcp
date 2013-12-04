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

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

TARGET = pmie$(EXECSUFFIX)

CFILES	= pmie.c symbol.c dstruct.c lexicon.c syntax.c pragmatics.c eval.c \
	  show.c match_inst.c systemlog.c stomp.c andor.c

HFILES  = fun.h dstruct.h eval.h lexicon.h pragmatics.h stats.h \
	  show.h symbol.h syntax.h systemlog.h stomp.h andor.h

SKELETAL = hdr.sk fetch.sk misc.sk aggregate.sk unary.sk binary.sk \
	merge.sk act.sk

LSRCFILES = $(SKELETAL) meta logger.h

LDIRT += $(YFILES:%.y=%.tab.?) fun.c fun.o $(TARGET) grammar.h

LLDLIBS = $(PCPLIB) $(LIB_FOR_MATH) $(LIB_FOR_REGEX)

LCFLAGS += $(PIECFLAGS)
LLDFLAGS += $(PIELDFLAGS)

default:	$(TARGET)

YFILES = grammar.y

.NOTPARALLEL:
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

fun.h: andor.h
andor.o dstruct.o eval.o fun.o grammar.tab.o lexicon.o match_inst.o pmie.o pragmatics.o show.o syntax.o systemlog.o: dstruct.h
dstruct.o eval.o pmie.o pragmatics.o syntax.o systemlog.o: eval.h
andor.o dstruct.o eval.o fun.o match_inst.o: fun.h
lexicon.o syntax.o: grammar.h
grammar.tab.o lexicon.o show.o syntax.o: lexicon.h
systemlog.o: logger.h
andor.o dstruct.o eval.o fun.o grammar.tab.o lexicon.o pmie.o pragmatics.o show.o syntax.o: pragmatics.h
andor.o dstruct.o eval.o fun.o grammar.tab.o match_inst.o pmie.o show.o syntax.o: show.h
andor.o fun.o grammar.tab.o pmie.o stomp.o: stomp.h
andor.o dstruct.o eval.o fun.o grammar.tab.o lexicon.o match_inst.o pmie.o pragmatics.o show.o symbol.o syntax.o systemlog.o: symbol.h
grammar.tab.o lexicon.o pmie.o syntax.o systemlog.o: syntax.h
fun.o grammar.tab.o systemlog.o: systemlog.h

