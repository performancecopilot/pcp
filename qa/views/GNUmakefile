TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/views
VIEWFILES = 006-01 006-02 006-03 006-04 006-05 \
	    AntiAliasing NoSuchHost RotateColors ScaleManual SpotInstances

default default_pcp setup:

install install_pcp: $(VIEWFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(VIEWFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

