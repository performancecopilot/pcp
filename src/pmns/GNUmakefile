#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

PMNS_VAR_DIR = $(PCP_VAR_DIR)/pmns
PMNS_BIN_DIR = $(PCP_BINADM_DIR)
PMNS_LIB_DIR = $(PCP_SHARE_DIR)/lib

# Take control here ... do not need to search in libpcp_pmda directory
# for libpcp_pmda DSO, and it is not even built yet for a virgin make.
#
PCPLIB_LDFLAGS = -L$(TOPDIR)/src/libpcp/src

CFILES  = pmnsmerge.c pmnsutil.c pmnsdel.c
HFILES  = pmnsutil.h
TARGETS = pmnsmerge$(EXECSUFFIX) pmnsdel$(EXECSUFFIX)
SCRIPTS = pmnsadd
LOCKERS	= lockpmns unlockpmns
STDPMID = stdpmid.pcp stdpmid.local

LSRCFILES = Make.stdpmid GNUmakefile.install Rebuild ReplacePmnsSubtree \
	    $(STDPMID) $(SCRIPTS) $(LOCKERS)

LLDLIBS = $(PCPLIB)
LDIRT	= *.log *.pmns stdpmid .NeedRebuild build.script $(TARGETS)

default:	$(SCRIPTS) $(LOCKERS) $(TARGETS) \
		GNUmakefile.install .NeedRebuild Rebuild ReplacePmnsSubtree stdpmid

include $(BUILDRULES)

pmnsmerge$(EXECSUFFIX):      pmnsmerge.o pmnsutil.o
	$(CCF) -o $@ $(LDFLAGS) pmnsmerge.o pmnsutil.o $(LDLIBS)

pmnsdel$(EXECSUFFIX):        pmnsdel.o pmnsutil.o
	$(CCF) -o $@ $(LDFLAGS) pmnsdel.o pmnsutil.o $(LDLIBS)

.NeedRebuild:
	echo "This file flags the rc scripts to rebuild the PMNS" > .NeedRebuild

# All PMNS config stuff goes in $PCP_VAR_DIR/pmns
# For platforms that want it, the .NeedRebuild hook is added there,
# else a manual touch(1) here is as close as it gets unfortunately.
#
install:	default
	$(INSTALL) -m 755 $(TARGETS) $(SCRIPTS) $(PMNS_BIN_DIR)
	$(INSTALL) -m 755 $(LOCKERS) ReplacePmnsSubtree $(PMNS_LIB_DIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMNS_VAR_DIR)/Makefile
	$(INSTALL) -m 755 Rebuild $(PMNS_VAR_DIR)/Rebuild
	$(INSTALL) -m 755 Make.stdpmid $(PMNS_VAR_DIR)/Make.stdpmid
	$(INSTALL) -m 644 $(STDPMID) $(PMNS_VAR_DIR)
ifeq (, $(filter redhat debian, $(PACKAGE_DISTRIBUTION)))
	$(INSTALL) -m 644 .NeedRebuild $(PMNS_VAR_DIR)/.NeedRebuild
endif

stdpmid:	$(STDPMID)
	rm -f build.script
	$(AWK) <Make.stdpmid >build.script '\
/^. \$$PCP_DIR/	{ print "PCP_CONF=../include/pcp.conf"; print ". ../include/pcp.env"; next }\
		{ print }'
	sh ./build.script

default_pcp:	default

install_pcp:	install

pmnsdel.o pmnsmerge.o pmnsutil.o:	$(TOPDIR)/src/include/pcp/libpcp.h
