TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/secure

CRASHFILES = $(shell echo crash-*)
QUERYFILES = $(shell echo query-*)
LEAKFILES = $(shell echo leak-*)
HANGFILES = $(shell echo hang-*)

PYMODULES = pcppdu.python
TESTCASES = $(CRASHFILES) $(QUERYFILES) $(LEAKFILES) $(HANGFILES)
LSRCFILES = $(TESTCASES) $(PYMODULES)
LDIRT = pcppdu.py pcppdu.pyc pcppdu.pyo

default default_pcp setup: pcppdu.py

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(TESTCASES) $(PYMODULES) $(TESTDIR)

include $(BUILDRULES)

%.py : %.python
	$(LN_S) $< $@
