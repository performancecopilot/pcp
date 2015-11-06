#!gmake
#
# Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/tmparch

include GNUlocaldefs

default default_pcp setup: $(MKARCH)

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 GNUlocaldefs $(TESTDIR)/GNUlocaldefs
	$(INSTALL) -m 644 $(MYCONFIGS) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TESTDIR)

include $(BUILDRULES)
