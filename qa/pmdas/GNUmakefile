#!gmake
#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2010 Aconex.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas
SUBDIRS = broken bigun dynamic slow slow_python
LSRCFILES = GNUmakefile.install

default default_pcp: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
