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

TESTDIR = $(PCP_VAR_DIR)/testsuite
SUBDIRS = broken bigun

default default_pcp: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)/pmdas
	$(INSTALL) -m 644 GNUmakefile $(TESTDIR)/pmdas/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
