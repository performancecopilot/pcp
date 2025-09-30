#
# Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/tmparch

default default_pcp:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f GNUlocaldefs $(TESTDIR)/GNUlocaldefs
	$(INSTALL) -m 644 -f $(MYCONFIGS) $(OTHERS) $(TESTDIR)
	$(INSTALL) -m 755 -f $(MYSCRIPTS) $(TESTDIR)

include GNUlocaldefs

setup: check_pmcd_stable $(MKARCH)

check_pmcd_stable:
	./check-pmcd-stable

LDIRT += mkbig1.log

include $(BUILDRULES)
