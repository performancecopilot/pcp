#!gmake
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
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
PCP_LIBS = -L$(TOPDIR)/src/libpcp/src

CFILES  = pmnsmerge.c pmnsutil.c pmnsdel.c
HFILES  = pmnsutil.h
TARGETS = pmnsmerge$(EXECSUFFIX) pmnsdel$(EXECSUFFIX)
SCRIPTS = pmnsadd
LOCKERS	= lockpmns unlockpmns

STDPMIDS= stdpmid.irix  stdpmid.linux stdpmid.add-on stdpmid.local \
	stdpmid.pcp stdpmid.qa

LSRCFILES = Make.stdpmid GNUmakefile.install Rebuild ReplacePmnsSubtree \
	    $(STDPMIDS) $(SCRIPTS) $(LOCKERS)

LLDLIBS = -lpcp
LDIRT	= *.log *.pmns stdpmid $(TARGETS)

default:	$(SCRIPTS) $(LOCKERS) $(TARGETS) \
		GNUmakefile.install Rebuild ReplacePmnsSubtree stdpmid

include $(BUILDRULES)

pmnsmerge$(EXECSUFFIX):      pmnsmerge.o pmnsutil.o
	$(CCF) -o $@ $(LDFLAGS) pmnsmerge.o pmnsutil.o $(LDLIBS)

pmnsdel$(EXECSUFFIX):        pmnsdel.o pmnsutil.o
	$(CCF) -o $@ $(LDFLAGS) pmnsdel.o pmnsutil.o $(LDLIBS)

# All PMNS config stuff goes in $PCP_VAR_DIR/pmns
# For platforms with packaging, the .NeedRebuild hook is added there,
# for the others touch(1) here is as close as it gets unfortunately.
#
install:	default
	$(INSTALL) -m 755 $(TARGETS) $(SCRIPTS) $(PMNS_BIN_DIR)
	$(INSTALL) -m 755 $(LOCKERS) ReplacePmnsSubtree $(PMNS_LIB_DIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMNS_VAR_DIR)/Makefile
	$(INSTALL) -m 755 Rebuild $(PMNS_VAR_DIR)/Rebuild
	$(INSTALL) -m 755 Make.stdpmid $(PMNS_VAR_DIR)/Make.stdpmid
	$(INSTALL) -m 644 $(STDPMIDS) $(PMNS_VAR_DIR)
ifeq "$(findstring $(TARGET_OS),linux darwin)" ""
	touch $(DIST_ROOT)$(PMNS_VAR_DIR)/.NeedRebuild
endif

stdpmid:	$(STDPMIDS)
	PCP_CONF=$(TOPDIR)/src/include/pcp.conf export PCP_CONF; \
	sed -e 's/\$PCP_DIR\/etc\/pcp\.env/\.\.\/\.\.\/src\/include\/pcp.env/' <Make.stdpmid >run; \
	sh ./run; rm -f run

default_pcp:	default

install_pcp:	install
