TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/linux
ROOTFILES = $(shell echo *-root-*.tgz *-root.tgz)
SYSFSFILES = $(shell echo sysfs-*-*-*.tgz sysfs-*-*.tgz sysfs-*-???)
ZFSFILES = $(shell echo zfs-stats.*.tgz)
CPUINFOFILES = $(shell echo cpuinfo-*)
PROCIRQFILES = $(shell echo interrupts-* softirqs-*)
PROCNETFILES = $(shell echo procnet-* proc_net_*)
PROCSERIALFILES = $(shell echo proc_serial_*)
PROCSYSFILES = $(shell echo procsys-*)
PROCPIDFILE = $(shell echo proc-*-pid-???.tgz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(ROOTFILES) $(SYSFSFILES) $(ZFSFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f $(PROCPIDFILE) $(PROCSERIALFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f $(PROCIRQFILES) $(CPUINFOFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f $(PROCNETFILES) $(PROCSYSFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

