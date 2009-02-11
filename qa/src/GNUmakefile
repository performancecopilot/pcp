TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = grind-tools

SUBDIRS = qmc_context qmc_desc qmc_dynamic qmc_format qmc_group \
	  qmc_hosts qmc_indom qmc_metric qmc_source dynamic

default install:
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
