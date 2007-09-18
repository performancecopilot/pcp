#!smake
#
# $Id: Makefile,v 1.17 2001/04/06 04:52:11 ajag Exp $
#

PCP_SRC_DEPTH = ../../..
include versiondefs
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS	= $(FROM_PROC) pmda$(IAM)

PROC_DIR = ../../proc
PROC_SRCDIR = $(PROC_DIR)/src

FROM_PROC_H = cluster.h pracinfo.h proc_aux.h pscred.h \
	      pstatus.h pmemory.h proc.h procmem.h psinfo.h \
	      psusage.h ctltab.h nameinfo.h
FROM_PROC_C = pmemory.c pracinfo.c proc_aux.c \
	      pscred.c psinfo.c pstatus.c psusage.c \
	      ttyname.c procmem.c nameinfo.c

FROM_PROC = $(FROM_PROC_C) $(FROM_PROC_H)
CFILES	= $(FROM_PROC_C) \
	  pglobal.c ctltab.c hotproc.c pcpu.c \
	  config.c gram_node.c error.c ppred_values.c

OBJECTS = gram.o lex.o $(CFILES:S/.c/.o/g)

LCOPTS	= -fullwarn
LCINCS  = $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp_pmda -lpcp -lc -ll

IAM	= hotproc
DOMAIN	= HOTPROC
LDIRT	= ../domain.h gram.h yy.lex.c y.tab.?

default: ../domain.h $(TARGETS)

include $(COMMONRULES)

install:	default

pmda$(IAM):	$(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D01 $@

#
# PROC_SRCDIR dependencies
#
cluster.h: $(PROC_SRCDIR)/cluster.h
	ln -s $? $@
pracinfo.h: $(PROC_SRCDIR)/pracinfo.h
	ln -s $? $@
pracinfo.c: $(PROC_SRCDIR)/pracinfo.c
	ln -s $? $@
proc_aux.h: $(PROC_SRCDIR)/proc_aux.h
	ln -s $? $@
proc_aux.c: $(PROC_SRCDIR)/proc_aux.c
	ln -s $? $@
pscred.h: $(PROC_SRCDIR)/pscred.h
	ln -s $? $@
pscred.c: $(PROC_SRCDIR)/pscred.c
	ln -s $? $@
pstatus.h: $(PROC_SRCDIR)/pstatus.h
	ln -s $? $@
pstatus.c: $(PROC_SRCDIR)/pstatus.c
	ln -s $? $@
pmemory.h: $(PROC_SRCDIR)/pmemory.h
	ln -s $? $@
pmemory.c: $(PROC_SRCDIR)/pmemory.c
	ln -s $? $@
proc.h: $(PROC_SRCDIR)/proc.h
	ln -s $? $@
procmem.h: $(PROC_SRCDIR)/procmem.h
	ln -s $? $@
procmem.c: $(PROC_SRCDIR)/procmem.c
	ln -s $? $@
psinfo.h: $(PROC_SRCDIR)/psinfo.h
	ln -s $? $@
psinfo.c: $(PROC_SRCDIR)/psinfo.c
	ln -s $? $@
psusage.h: $(PROC_SRCDIR)/psusage.h
	ln -s $? $@
psusage.c: $(PROC_SRCDIR)/psusage.c
	ln -s $? $@
ttyname.c: $(PROC_SRCDIR)/ttyname.c
	ln -s $? $@
ctltab.h: $(PROC_SRCDIR)/ctltab.h
	ln -s $? $@
nameinfo.h: $(PROC_SRCDIR)/nameinfo.h
	ln -s $? $@
nameinfo.c: $(PROC_SRCDIR)/nameinfo.c
	ln -s $? $@


# grammar stuff

lex.o gram.o config.o:	gram.h

gram.h:	gram.y
	$(YACC) -d gram.y
	-if cmp -s y.tab.h $@; then rm y.tab.h; touch gram.h; else mv y.tab.h $@; fi

.NOTPARALLEL:
.ORDER: ../domain.h $(OBJECTS)

../domain.h:
	cd ..; make domain.h
