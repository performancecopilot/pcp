TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/views
VIEWFILES = 006-01 006-02 006-03 006-04 006-05 \
	    AntiAliasing NoSuchHost RotateColors ScaleManual
LSRCFILES = $(VIEWFILES) GNUmakefile.install

default setup:

install: $(VIEWFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(VIEWFILES) $(TESTDIR)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
