TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/db2local
MYFILES = db2diag-error.txt db2diag-severe.txt db2level.txt \
	db2-list-db-directory-alias.txt db2-list-db-directory-remote.txt \
	db2-list-db-directory.txt \
	db2pd-agents.txt db2pd-bufferpools.txt db2pd-inst.txt \
	db2pd-tablespaces.txt

default setup default_pcp:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
