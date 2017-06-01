TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/linux
ROOTFILES = $(shell echo *-root-*.tgz)
SYSFSFILES = $(shell echo sysfs-*-*.tgz)
CPUINFOFILES = $(shell echo cpuinfo-*)
PROCIRQFILES = $(shell echo interrupts-* softirqs-*)
PROCNETFILES = $(shell echo procnet-* proc_net_*)
PROCSYSFILES = $(shell echo procsys-*)
BIGPIDFILE = $(shell echo proc-big-pid-001.tgz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(ROOTFILES) $(SYSFSFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(CPUINFOFILES) $(BIGPIDFILE) $(TESTDIR)
	$(INSTALL) -m 644 $(PROCIRQFILES) $(PROCNETFILES) $(PROCSYSFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

