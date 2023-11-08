TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/farm
ROOTFILES = $(shell echo *-root-*.tgz)
SCRIPTS = lsblk.sh smartctl.sh

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f README $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

