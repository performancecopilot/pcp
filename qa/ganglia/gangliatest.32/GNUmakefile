TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

GTEST = gangliatest.32

TESTDIR = $(PCP_VAR_DIR)/testsuite/ganglia/$(GTEST)

QAFILES = $(shell echo *.rrd)

LSRCFILES = $(QAFILES)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(QAFILES) $(TESTDIR)

include $(BUILDRULES)
