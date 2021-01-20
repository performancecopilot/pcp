TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/hacluster
ROOTFILES = $(shell echo *-root-*.tgz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)

include $(BUILDRULES)

