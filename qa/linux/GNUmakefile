TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/linux
ROOTFILES = cgroups-root-001.tgz
LSRCFILES = $(VIEWFILES) GNUmakefile.install

default setup:

install: $(VIEWFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	#$(INSTALL) -m 644 $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(ROOTFILES) $(TESTDIR)/$(ROOTFILES)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

default_pcp:

install_pcp:
