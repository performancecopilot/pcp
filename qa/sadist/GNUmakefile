#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/sadist

SADBIN = fche-csb6-rhel6.bin
SADXML = fche-csb6-rhel6.xml smash-rhel6.xml

LSRCFILES = $(SADBIN) $(SADXML)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(SADBIN) $(SADXML) $(TESTDIR)

include $(BUILDRULES)
