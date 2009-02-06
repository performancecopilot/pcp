#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

MAN_SECTION	= 3

MAN_PAGES	= $(shell echo *.$(MAN_SECTION))
MAN_DEST	= $(PKG_MAN_DIR)/man$(MAN_SECTION)
LSRCFILES	= $(MAN_PAGES)

default : $(MAN_PAGES)

include $(BUILDRULES)

install : default
	$(INSTALL) -m 755 -d $(MAN_DEST)
	$(INSTALL_MAN)
