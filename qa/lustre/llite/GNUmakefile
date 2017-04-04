TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

LSUBSYS = llite

TESTDIR = $(PCP_VAR_DIR)/testsuite/lustre/$(LSUBSYS)

SUBDIRS = lustre-ffff880378305c00

default setup default_pcp: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
