TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/src
SUBDIRS = qmc_context qmc_desc qmc_dynamic qmc_format qmc_group \
	  qmc_hosts qmc_indom qmc_metric qmc_source dynamic

SCRIPTS = grind-tools
LSRCFILES = $(SCRIPTS)

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
