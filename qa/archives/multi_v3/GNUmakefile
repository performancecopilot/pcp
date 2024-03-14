TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives/multi_v3
SRCARCH = $(shell echo *.[0-9])
SRCMETA = $(patsubst %.0,%.meta,$(SRCARCH))
SRCIDX = $(patsubst %.0,%.index,$(SRCARCH))

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCARCH) $(SRCMETA) $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
