TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives/multi_mixed
SRCARCH = $(shell echo *.0)
SRCMETA = $(patsubst %.0,%.meta,$(SRCARCH))
SRCIDX = $(patsubst %.0,%.index,$(SRCARCH))
XZSRCARCH = $(shell echo *.0.xz)
XZSRCMETA = $(patsubst %.0.xz,%.meta.xz,$(XZSRCARCH))
XZSRCIDX = $(patsubst %.0.xz,%.index,$(XZSRCARCH))
LDIRT = pmlogger.log*

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCARCH) $(SRCMETA) $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 -f $(XZSRCARCH) $(XZSRCMETA) $(XZSRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
