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

setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/qa/pmdas
	$(INSTALL) -m 644 GNUmakefile $(PCP_VAR_DIR)/qa/pmdas/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
