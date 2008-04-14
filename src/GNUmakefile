TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = include libqmc libqwt libqwt3d chart dumptext query snap time

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
