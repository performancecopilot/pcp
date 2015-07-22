#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/sadist

SADBIN = fche-csb6-rhel6.bin 859117-rhel5.bin f16-sa20.bin \
	 rhel5-sa20.bin
SADXML = fche-csb6-rhel6.xml smash-rhel6.xml smash-rhel5.xml \
	 859117-rhel5.xml 891688-dash-time.xml

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(SADBIN) $(SADXML) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
