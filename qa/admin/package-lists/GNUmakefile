TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/admin/package-lists
SCRIPTS	= new
DOCS	= README
DATA	= $(shell echo *+*+*)

default setup default_pcp:

install install_pcp: $(SCRIPTS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f $(DATA) $(DOCS) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

