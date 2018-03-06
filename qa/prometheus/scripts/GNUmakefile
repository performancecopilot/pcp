TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/prometheus/scripts
SCRIPTS = $(shell ls -1 | sed -e '/^GNU.*/d' -e '/^curl$$/d' -e '/not_exec/d' -e '/.*.txt$$/d')
NONEXEC = $(shell echo *not_exec* *.txt)
SUBDIRS = curl

default setup default_pcp:	$(SCRIPTS) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp:	$(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 $(NONEXEC) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

