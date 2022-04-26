TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/denki
ROOTFILES = bigsystem.tgz Thinkpad_L480_bat_busy.tgz apple_silicon_macbook.tgz Thinkpad_T460s_2_batteries.tgz

default setup default_pcp:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 -f $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

