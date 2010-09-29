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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PERLMOD   = PMDA.pm
INTERFACE = PMDA.xs local.h local.c typemap
TESTCODE  = test.pl cvalue.c
PERLDOCS  = Changes MANIFEST
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LPKGDIRT = PCP-PMDA-*
LDIRT = Makefile COPYING PMDA.bs PMDA.c PMDA.o cvalue pm_to_blib blib \
	Makefile.old local.o $(LPKGDIRT)

default: dist

MAKEMAKER_OPTIONS = "INSTALLDIRS=vendor"

INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT PREFIX=/usr

ifeq ($(TARGET_OS),mingw)
PERLMAKE = dmake.exe
else
PERLMAKE = $(MAKE)
endif

PMDA.o: Makefile PMDA.xs
	$(PERLMAKE) -f Makefile

Makefile: COPYING Makefile.PL
	PCP_TOPDIR=`pwd`/$(TOPDIR); export PCP_TOPDIR;  \
	CC="$(CC) $(CFLAGS_ABI)" perl Makefile.PL $(MAKEMAKER_OPTIONS)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: PMDA.o
	rm -rf $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
	$(PERLMAKE) -f Makefile install DESTDIR=$$DIST_ROOT
	find $$DIST_ROOT -name .packlist -exec mv '{}' $(TOPDIR)/perl-pcp-pmda.list ';'
	sed -e s@$$DIST_ROOT@@ -e 's/\.[0-9]pm$$/&.gz/' -e '/\.bs$$/d' -i.tmp $(TOPDIR)/perl-pcp-pmda.list
	find $$DIST_ROOT -name perllocal.pod -exec rm -f '{}' ';'
	find $$DIST_ROOT -name \*.bs  -exec rm -f '{}' ';'
	find $$DIST_ROOT -name PMDA.so -exec chmod 755 '{}' ';'
endif

install_perl:
	$(PERLMAKE) -f Makefile install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

