TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/linux
ROOTFILES = $(shell echo *-root-*.tgz)
CPUINFOFILES = $(shell echo cpuinfo-*)
PROCNETFILES = $(shell echo procnet-*)
BIGPIDFILE = $(shell echo proc-big-pid-001.tgz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(CPUINFOFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(PROCNETFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(BIGPIDFILE) $(TESTDIR)/$(BIGPIDFILE)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

