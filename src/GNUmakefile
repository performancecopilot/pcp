TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = include libpcp_qmc libpcp_qwt \
	pmafm pmchart pmdumptext pmquery pmsnap pmtime

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
