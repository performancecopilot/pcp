TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/mic

PYMODULES = micmgmt.python

LSRCFILES = $(PYMODULES)
LDIRT = micmgmt.py micmgmt.pyc micmgmt.pyo

default default_pcp setup: micmgmt.py

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(PYMODULES) $(TESTDIR)/$(PYMODULES)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

%.py : %.python
	$(LN_S) $< $@
