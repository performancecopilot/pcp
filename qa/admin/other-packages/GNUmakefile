TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/admin/other-packages
DATA	= manifest unavailable skip skip.*

default setup default_pcp:

install install_pcp: $(SCRIPTS)
	$(INSTALL) -m 644 $(DATA) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

