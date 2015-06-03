#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/lustre

SUBDIRS = llite lnet

default setup default_pcp: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
