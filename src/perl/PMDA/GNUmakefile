#!gmake
#
# Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

PERLMOD   = PMDA.pm
INTERFACE = PMDA.xs local.h local.c typemap
TESTCODE  = test.pl cvalue.c
PERLDOCS  = Changes MANIFEST
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LPKGDIRT = PCP-PMDA-*.tar.gz MYMETA.yml MYMETA.json
LDIRDIRT = PCP-PMDA-*
LDIRT = Makefile COPYING PMDA.bs PMDA.c PMDA.o cvalue pm_to_blib blib \
	Makefile.old local.o $(LPKGDIRT)

default: dist

MAKEMAKER_OPTIONS = INSTALLDIRS=$(PERL_INSTALLDIRS) INSTALLVENDORMAN3DIR=$(PCP_MAN_DIR)/man3
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT

ifeq ($(TARGET_OS),mingw)
PERLMAKE = dmake.exe
else
PERLMAKE = $(MAKE)
endif

PMDA.o: Makefile PMDA.xs
	$(PERLMAKE) -f Makefile

Makefile: COPYING Makefile.PL
	$(call PERL_MAKE_MAKEFILE)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: PMDA.o
	rm -rf $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
ifeq "$(PACKAGE_DISTRIBUTION)" "openbsd"
	$(call PERL_GET_FILELIST,pure_perl_install,$(TOPDIR)/perl-pcp-pmda.list,PMDA)
else
	$(call PERL_GET_FILELIST,install,$(TOPDIR)/perl-pcp-pmda.list,PMDA)
endif
endif

install_perl:
	$(PERLMAKE) -f Makefile pure_install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

