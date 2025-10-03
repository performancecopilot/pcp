TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/opentelemetry/scripts/curl
SCRIPTS = $(shell ls -1 | sed -e '/^GNU.*/d')

default setup default_pcp:	$(SCRIPTS)

install install_pcp:	$(SCRIPTS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)/$(SCRIPTS)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

