#!gmake
#
# Copyright (c) 2010 Aconex.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
PATH	= $(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

SUBDIRS = broken bigun

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

clobber clean install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
