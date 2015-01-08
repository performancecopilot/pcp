TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/lustre

SUBDIRS = llite lnet

default setup default_pcp: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
