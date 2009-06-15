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
TESTCODE  = test.pl
PERLDOCS  = Changes MANIFEST
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LPKGDIRT = PCP-MMV-*
LDIRT = Makefile COPYING MMV.bs MMV.c MMV.o pm_to_blib blib \
	Makefile.old $(LPKGDIRT)

default: dist

ifeq ($(PACKAGE_DISTRIBUTION),debian)
MAKEMAKER_OPTIONS = "INSTALLDIRS=vendor"
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT PREFIX=/usr
endif
ifeq ($(TARGET_OS),mingw)
PERLMAKE = dmake.exe
else
PERLMAKE = $(MAKE)
endif

MMV.o: Makefile MMV.xs
	$(PERLMAKE) -f Makefile

Makefile: COPYING
	perl Makefile.PL $(MAKEMAKER_OPTIONS)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: MMV.o
	rm -f $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default

install_perl:
	$(PERLMAKE) -f Makefile install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

