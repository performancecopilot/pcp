#
# Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = INSTALL PORTING CHANGES COPYING
LDIRT = *.gz

default: CHANGES.gz

include $(BUILDRULES)

CHANGES.gz:
	$(ZIP) --best -c < CHANGES > $@

install: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -S CHANGES.gz $(PKG_DOC_DIR)/changelog.gz
else
	$(INSTALL) -m 644 COPYING $(PKG_DOC_DIR)/COPYING
endif

install-dev:
