TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives/multi-xz-1k
SRCARCH = $(shell echo *.[0-9].xz)
SRCMETA = $(shell echo *.meta*)
SRCIDX = $(shell echo *.index*)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(SRCARCH) $(SRCMETA) $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

