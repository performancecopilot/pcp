TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/views
VIEWFILES = 006-01 006-02 006-03 006-04 006-05 \
	    AntiAliasing NoSuchHost RotateColors ScaleManual
LSRCFILES = $(VIEWFILES)

default:

install: $(VIEWFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(VIEWFILES) $(TESTDIR)

include $(BUILDRULES)
