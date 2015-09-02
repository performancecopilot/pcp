TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/linux
ROOTFILES = $(shell echo *-root-*.tgz)
CPUINFOFILES = $(shell echo cpuinfo-*)
PROCNETFILES = $(shell echo procnet-*)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(CPUINFOFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(PROCNETFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

