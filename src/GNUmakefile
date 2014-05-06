TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = include libqmc libqwt pmchart pmdumptext pmafm pmquery pmsnap pmtime

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
