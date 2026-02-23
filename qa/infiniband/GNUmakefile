TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/infiniband
ROOTFILES = $(shell echo ib-root-*.tgz)
SCRIPT	= fake-rdma
LDIRT`	= rdma

default setup default_pcp: rdma

rdma:	fake-rdma
	@rm -f rdma
	$(LN_S) fake-rdma rdma

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 755 -t $(TESTDIR)/rdma -f $(SCRIPT) $(TESTDIR)/$(SCRIPT)

include $(BUILDRULES)
