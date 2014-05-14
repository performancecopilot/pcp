#
# Copyright (c) 2013 Red Hat.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = PCP_UAG PCP_PG PCP_TCS
LSRCFILES = README

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

default_pcp : default

install_pcp : install
