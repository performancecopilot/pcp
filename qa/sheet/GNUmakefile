TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/sheet

TESTDATA = simple.csv simple.ods simple.sxc simple.xls simple.xlsx \
	   simple-resave.csv simple-resave.xlsx \
	   pcp2xlsx.qa.1132.out

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(TESTDATA) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
