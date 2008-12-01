#!gmake
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

PERLMOD   = LogSummary.pm
PERLDOCS  = Changes MANIFEST README
PERLCODE  = extract.pl exceldemo.pl
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(PERLCODE)
SUBDIRS   = t

LPKGDIRT = PCP-LogSummary-*
LDIRT = Makefile COPYING pm_to_blib blib Makefile.old $(LPKGDIRT) *.xls

ifeq "$(findstring $(TARGET_OS),cygwin mingw)" ""
default: dist
else
default:
endif

ifeq ($(PACKAGE_DISTRIBUTION),debian)
MAKEMAKER_OPTIONS = "INSTALLDIRS=vendor"
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT PREFIX=/usr
endif

Makefile: COPYING
	perl Makefile.PL $(MAKEMAKER_OPTIONS)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: Makefile
	rm -f $(LPKGDIRT)
	$(MAKE) -f Makefile $@

include $(BUILDRULES)

install: default

install_perl:
	$(MAKE) -f Makefile install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

