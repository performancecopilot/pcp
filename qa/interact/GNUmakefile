TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/interact
TESTCASES = $(shell echo *-testcases)
LSRCFILES = $(TESTCASES) GNUmakefile.install

default setup:

install: $(TESTCASES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(TESTCASES) $(TESTDIR)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
