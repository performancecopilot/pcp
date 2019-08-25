TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/bpftrace
PYTESTFILE = tests.py

ifeq "$(PMDA_BPFTRACE)" "true"
default setup default_pcp:

install install_pcp: default $(PYTESTFILE)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(PYTESTFILE) $(TESTDIR)/$(PYTESTFILE)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)
