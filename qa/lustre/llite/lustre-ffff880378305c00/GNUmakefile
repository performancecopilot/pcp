TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

LSUBSYS = llite
LLITE_MOUNT = lustre-ffff880378305c00

TESTDIR = $(PCP_VAR_DIR)/testsuite/lustre/$(LSUBSYS)/$(LLITE_MOUNT)

QAFILES = stats

LSRCFILES = $(QAFILES)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(QAFILES) $(TESTDIR)/$(QAFILES)

include $(BUILDRULES)
