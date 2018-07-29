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

LPKGDIRT = PCP-LogSummary-*.tar.gz MYMETA.yml MYMETA.json
LDIRDIRT = PCP-LogSummary-*
LDIRT = Makefile COPYING pm_to_blib blib Makefile.old $(LPKGDIRT) *.xls

default: dist

MAKEMAKER_OPTIONS = INSTALLDIRS=$(PERL_INSTALLDIRS) INSTALLVENDORMAN3DIR=$(PCP_MAN_DIR)/man3
INSTALLER_OPTIONS = DESTDIR=$$DIST_ROOT

PERLMAKE = $(MAKE)

Makefile: COPYING Makefile.PL
	$(call PERL_MAKE_MAKEFILE)

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test dist: Makefile
	rm -f $(LPKGDIRT)
	$(PERLMAKE) -f Makefile $@

include $(BUILDRULES)

install: default
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
ifeq "$(PACKAGE_DISTRIBUTION)" "openbsd"
	$(call PERL_GET_FILELIST,pure_perl_install,$(TOPDIR)/perl-pcp-logsummary.list,LogSummary)
else
	$(call PERL_GET_FILELIST,install,$(TOPDIR)/perl-pcp-logsummary.list,LogSummary)
endif
endif

install_perl:
	$(PERLMAKE) -f Makefile pure_install $(INSTALLER_OPTIONS)

default_pcp: default

install_pcp: install

