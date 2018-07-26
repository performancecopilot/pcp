#!gmake
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

PERLMOD   = LogImport.pm
INTERFACE = LogImport.xs typemap
TESTCODE  = 
PERLDOCS  = Changes MANIFEST
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LPKGDIRT = PCP-LogImport-*.tar.gz tmp MYMETA.yml MYMETA.json
LDIRDIRT = PCP-LogImport-*
LDIRT = Makefile COPYING LogImport.bs LogImport.c LogImport.o cvalue pm_to_blib blib \
	Makefile.old $(LPKGDIRT)

default: dist

MAKEMAKER_OPTIONS = INSTALLDIRS=$(PERL_INSTALLDIRS) INSTALLVENDORMAN3DIR=$(PCP_MAN_DIR)/man3
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT

PERLMAKE = $(MAKE)

LogImport.o: Makefile LogImport.xs typemap
	$(PERLMAKE) -f Makefile

Makefile: COPYING Makefile.PL
	$(call PERL_MAKE_MAKEFILE)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: LogImport.o
	rm -rf $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
ifeq "$(PACKAGE_DISTRIBUTION)" "openbsd"
	$(call PERL_GET_FILELIST,pure_perl_install,$(TOPDIR)/perl-pcp-logimport.list,LogImport)
else
	$(call PERL_GET_FILELIST,install,$(TOPDIR)/perl-pcp-logimport.list,LogImport)
endif
endif

install_perl:
	$(PERLMAKE) -f Makefile pure_install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

LogImport.o:	$(TOPDIR)/src/include/pcp/libpcp.h
