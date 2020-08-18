TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/bpftrace
PYTESTFILES = $(shell echo *_tests.py)

ifeq "$(PMDA_BPFTRACE)" "true"
default setup default_pcp:

install install_pcp: default $(PYTESTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(PYTESTFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)
