#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/gluster

SCRIPT = test.sh
INFOFILES = $(shell echo info-*)
PROFFILES = $(shell echo profile-*)

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 644 $(INFOFILES) $(PROFFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

%.py : %.python
	$(LN_S) $< $@
