#!gmake
#
# Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
#
# $Id$
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LLDLIBS = -lpcp
CFILES	= pmsleep.c
CMDTARGET = pmsleep$(EXECSUFFIX)
LDIRT = $(TARGET)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install
