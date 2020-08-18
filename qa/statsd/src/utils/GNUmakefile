TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/statsd/src/utils

QAFILES = pmdastatsd_test_utils.py

ifeq "$(PMDA_STATSD)" "true"
default setup default_pcp:

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(QAFILES) $(TESTDIR)/$(QAFILES)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)
