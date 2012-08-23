#!gmake

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pconf/global
CONFIGS = pcp_eoe sss
LSRCFILES = $(CONFIGS)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CONFIGS) $(TESTDIR)

include $(BUILDRULES)
