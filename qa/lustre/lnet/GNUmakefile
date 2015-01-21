TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

LSUBSYS = lnet

TESTDIR = $(PCP_VAR_DIR)/testsuite/lustre/$(LSUBSYS)

QAFILES = stats

LSRCFILES = $(QAFILES)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(QAFILES) $(TESTDIR)/$(QAFILES)

include $(BUILDRULES)
