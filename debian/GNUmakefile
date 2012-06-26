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

PCPQA = pcpqa
PCPQA_FILES =

SUBDIRS = source
LDIRT = *.debhelper *.substvars *.log pcp files $(PCPQA)
LSRCFILES = changelog copyright control rules $(PCPQA_FILES)

default:

include $(BUILDRULES)

install: $(SUBDIRS)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/qa/debian
	$(INSTALL) -m 644 GNUmakefile changelog control copyright $(PCP_VAR_DIR)/qa/debian
	$(INSTALL) -m 755 rules $(PCP_VAR_DIR)/qa/debian/rules
	$(SUBDIRS_MAKERULE)
endif

setup:
