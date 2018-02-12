TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/prometheus/scripts/curl
SCRIPTS = $(shell ls -1 | sed -e '/^GNU.*/d')

default setup default_pcp:	$(SCRIPTS)

install install_pcp:	$(SCRIPTS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)/$(SCRIPTS)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

