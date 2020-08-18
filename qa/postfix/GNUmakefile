TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SCRIPT = qshape.sh
TESTDIR = $(PCP_VAR_DIR)/testsuite/postfix
LOGFILES = $(shell echo *-log-*.gz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 644 -f $(LOGFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

