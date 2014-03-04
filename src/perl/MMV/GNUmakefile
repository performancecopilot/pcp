#!gmake
#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

PERLMOD   = MMV.pm
INTERFACE = MMV.xs typemap
TESTCODE  = test.pl server.pl
PERLDOCS  = Changes MANIFEST
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LPKGDIRT = PCP-MMV-* MYMETA.yml MYMETA.json
LDIRT = Makefile COPYING MMV.bs MMV.c MMV.o pm_to_blib blib \
	Makefile.old $(LPKGDIRT)

default: dist

MAKEMAKER_OPTIONS = INSTALLDIRS=$(PERL_INSTALLDIRS) INSTALLVENDORMAN3DIR=$(PCP_MAN_DIR)/man3
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT

ifeq ($(TARGET_OS),mingw)
PERLMAKE = dmake.exe
else
PERLMAKE = $(MAKE)
endif

MMV.o: Makefile MMV.xs
	$(PERLMAKE) -f Makefile

Makefile: COPYING Makefile.PL
	$(call PERL_MAKE_MAKEFILE)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: MMV.o
	rm -f $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
	$(call PERL_GET_FILELIST,$(TOPDIR)/perl-pcp-mmv.list,MMV)
	find $$DIST_ROOT -name server.pl -exec chmod 755 '{}' ';'
endif

install_perl:
	$(PERLMAKE) -f Makefile pure_install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

