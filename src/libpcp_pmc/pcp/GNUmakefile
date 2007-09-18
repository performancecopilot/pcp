#! gmake

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

SYMLINKS = platform_defs.h pmapi.h impl.h
LDIRT = $(SYMLINKS)

SUBDIRS = pmc

default install : $(SYMLINKS) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

$(SYMLINKS):
	$(LN_S) $(TOPDIR)/src/include/$*.h .

include $(TOPDIR)/src/include/buildrules

default_pcp : default

install_pcp : install
