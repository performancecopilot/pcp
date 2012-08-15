TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/src
SUBDIRS = qmc_context qmc_desc qmc_dynamic qmc_format qmc_group \
	  qmc_hosts qmc_indom qmc_metric qmc_source dynamic

SCRIPT = grind-tools
LSRCFILES = $(SCRIPT) GNUmakefile.install

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup:

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
