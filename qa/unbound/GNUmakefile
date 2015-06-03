#!gmake 

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/unbound

SCRIPT = test.sh
METRICS = metrics.list
STATSFILES = $(shell echo unbound-control-stats-*)

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 644 $(STATSFILES) $(METRICS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

%.py : %.python
	(LN_S) $< $@
