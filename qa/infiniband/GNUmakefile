TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/infiniband
ROOTFILES = $(shell echo ib-root-*.tgz)

default setup default_pcp:

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(ROOTFILES) $(TESTDIR)

include $(BUILDRULES)
