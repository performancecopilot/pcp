TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives/multi-corrupted
SRCARCH = $(shell echo *.[0-9].xz)
SRCMETA = $(patsubst %.0.xz,%.meta.xz,$(SRCARCH))
SRCIDX = $(patsubst %.0.xz,%.index,$(SRCARCH))

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCARCH) $(SRCMETA) $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

