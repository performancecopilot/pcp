#
# Copyright (c) 2013 Red Hat.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = PCP_PG PCP_UAG
LSRCFILES = README

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

