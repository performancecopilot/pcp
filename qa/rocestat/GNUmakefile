#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/rocestat
MYFILES = rocestat.json metrics.list
PYMODULES = rocestat_test.python
LDIRT = rocestat_test.py rocestat_test.pyc rocestat_test.pyo

default default_pcp setup: rocestat_test.py

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 -f $(PYMODULES) $(TESTDIR)/$(PYMODULES)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

%.py : %.python
	$(LN_S) $< $@
