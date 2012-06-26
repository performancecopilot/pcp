#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
ifeq ($(shell [ -d /usr/gnu/bin ] && echo 0),0)
PATH	= /usr/gun/bin:$(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
else
PATH	= $(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
endif
include $(PCP_INC_DIR)/builddefs

LSRCFILES = format

default:

include $(BUILDRULES)

install:
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/qa/debian/source
	$(INSTALL) -m 644 GNUmakefile $(LSRCFILES) $(PCP_VAR_DIR)/qa/debian/source
endif

setup:
