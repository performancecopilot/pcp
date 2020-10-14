TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/admin
SCRIPTS = allow-pmlc-access check-vm check-manifest get_osname whatami \
	  list-packages
FILES	= dailyrc packages.rc README
SUBDIRS	= other-packages package-lists

default default_pcp setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f $(FILES) $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

