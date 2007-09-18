#! gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = pcp src examples

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(TOPDIR)/src/include/buildrules

default_pcp : default

install_pcp : install
