TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/admin
SCRIPTS = allow-pmlc-access check-vm check-manifest get_osname whatami
SUBDIRS	= other-packages

default default_pcp setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

