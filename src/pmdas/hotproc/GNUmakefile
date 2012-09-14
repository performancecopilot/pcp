#
# Copyright (c) 1995-2002,2007 Silicon Graphics, Inc.  All Rights Reserved.
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
include $(TOPDIR)/src/include/builddefs

# This is just to get the source exported to the community.
# 'Makefile' in particular is included only for clues it might offer,
# and should almost certainly be removed from SRCFILES once
# the pmda is working.
LSRCFILES = fixpmns.awk general.pmie help.fmt Install \
	    pmns.hotproc Remove sample.conf \
	    general.conf help.hotproc Makefile README root 
SUBDIRS = src

default default_pcp: 
install install_pcp:
include $(BUILDRULES)

# Remove # from start of all following lines to get pre-existing
# (but currently non-functional) content

#IAM	= hotproc
#DOMAIN	= HOTPROC
#
#TARGETS	= $(IAM) pmns help 
#
#PROC_DIR = ../linux
#PROC_SRCDIR = $(PROC_DIR)/src
#
#FROM_PROC_H = cluster.h pracinfo.h proc_aux.h pscred.h \
#	      pstatus.h pmemory.h proc.h procmem.h psinfo.h \
#	      psusage.h ctltab.h nameinfo.h
#FROM_PROC_C = pmemory.c pracinfo.c proc_aux.c \
#	      pscred.c psinfo.c pstatus.c psusage.c \
#	      ttyname.c procmem.c nameinfo.c
#
#FROM_PROC = $(FROM_PROC_C) $(FROM_PROC_H)
#
#CFILES	= $(FROM_PROC_C) \
#	  pglobal.c ctltab.c hotproc.c pcpu.c \
#	  config.c gram_node.c error.c ppred_values.c
#
#OBJECTS = gram.o lex.o $(CFILES:S/.c/.o/g)
#
#LCOPTS	= -fullwarn
#LCINCS  = $(PCP_INC_PATH)
#LCDEFS	= $(DEBUG)
#LLDOPTS	= $(PCP_LIBS_PATH)
#LLDLIBS	= $(PCP_PMDALIB)
#
#PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
#LDIRT	= domain.h *.log *.dir *.pag 
#
#default: $(TARGETS) check_help_src
#
#include $(TOPDIR)/src/include/buildrules.pro
#
#install: default
#	$(INSTALL) -m 755 -d $(PMDADIR)
#	$(INSTALL) -lns ../../..$(PMDADIR)/pmda$(IAM) pmda$(IAM) $(PCP_SHARE_DIR)/lib
#	$(INSTALL) -lns ../../..$(PMDADIR) $(IAM) $(PCP_PMDAS_DIR)
#	$(INSTALL) -m 755 pmda$(IAM) $(PMDADIR)
#	$(INSTALL) -m 755 Install Remove $(PMDADIR)
#	$(INSTALL) -m 644 README root help pmns domain.h sample.conf general.conf general.pmie $(PMDADIR)
#
#$(IAM):	$(OBJECTS)
#
#domain.h: ../../pmns/stdpmid
#	$(DOMAIN_MAKERULE)
#
#help: $(PROC_DIR)/help help.hotproc pmns
#	$(AWK) <$(PROC_DIR)/help '\
#$1 == "@"			{ want=0 }\
#$1 == "@" && $2 ~ /^proc/	{ want=1 }\
#want == 1	{ print }' \
#	| sed -e 's/proc\./hotproc./g' \
#	      -e 's/number of processes/number of "interesting" processes/g' \
#	| cat - help.hotproc \
#	| ./help.fmt >$@ 
#
#pmns: $(PROC_DIR)/root_linux pmns.hotproc fixpmns.awk
#	$(AWK) < $(PROC_DIR)/root_linux -f fixpmns.awk \
#	| sed -e '/nprocs/d' -e 's/60:/HOTPROC:/g' -e 's/^proc/hotproc/g' \
#	| cat - pmns.hotproc >$@
#
#hotproc.o: domain.h
#
#config.o: gram.tab.h
#
#check_help_src:	domain.h help root pmns
#	PCP_SRC_DEPTH=$(PCP_SRC_DEPTH) $(PCP_SRC_DEPTH)/buildtools/check_help_src help root
#
##
## PROC_SRCDIR dependencies
##
#cluster.h: $(PROC_SRCDIR)/cluster.h
#	ln -s $? $@
#pracinfo.h: $(PROC_SRCDIR)/pracinfo.h
#	ln -s $? $@
#pracinfo.c: $(PROC_SRCDIR)/pracinfo.c
#	ln -s $? $@
#proc_aux.h: $(PROC_SRCDIR)/proc_aux.h
#	ln -s $? $@
#proc_aux.c: $(PROC_SRCDIR)/proc_aux.c
#	ln -s $? $@
#pscred.h: $(PROC_SRCDIR)/pscred.h
#	ln -s $? $@
#pscred.c: $(PROC_SRCDIR)/pscred.c
#	ln -s $? $@
#pstatus.h: $(PROC_SRCDIR)/pstatus.h
#	ln -s $? $@
#pstatus.c: $(PROC_SRCDIR)/pstatus.c
#	ln -s $? $@
#pmemory.h: $(PROC_SRCDIR)/pmemory.h
#	ln -s $? $@
#pmemory.c: $(PROC_SRCDIR)/pmemory.c
#	ln -s $? $@
#proc.h: $(PROC_SRCDIR)/proc.h
#	ln -s $? $@
#procmem.h: $(PROC_SRCDIR)/procmem.h
#	ln -s $? $@
#procmem.c: $(PROC_SRCDIR)/procmem.c
#	ln -s $? $@
#psinfo.h: $(PROC_SRCDIR)/psinfo.h
#	ln -s $? $@
#psinfo.c: $(PROC_SRCDIR)/psinfo.c
#	ln -s $? $@
#psusage.h: $(PROC_SRCDIR)/psusage.h
#	ln -s $? $@
#psusage.c: $(PROC_SRCDIR)/psusage.c
#	ln -s $? $@
#ttyname.c: $(PROC_SRCDIR)/ttyname.c
#	ln -s $? $@
#ctltab.h: $(PROC_SRCDIR)/ctltab.h
#	ln -s $? $@
#nameinfo.h: $(PROC_SRCDIR)/nameinfo.h
#	ln -s $? $@
#nameinfo.c: $(PROC_SRCDIR)/nameinfo.c
#	ln -s $? $@
#
#
#
#
#default_pro : default
#
