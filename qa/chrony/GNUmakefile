TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/chrony

SCRIPT = chronyc
DATAFILES = tracking.dat sources.dat sourcestats.dat

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 644 -f $(DATAFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
