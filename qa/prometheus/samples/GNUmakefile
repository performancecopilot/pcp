TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/prometheus/samples
SAMPLES = $(shell echo *.txt)

default setup default_pcp:

install install_pcp: $(SAMPLES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(SAMPLES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

