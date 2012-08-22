TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = include libqmc libqwt chart dumptext pmafm query snap time

default install clean: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
