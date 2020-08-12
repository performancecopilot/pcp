#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020 Red Hat.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= mailq
DOMAIN	= MAILQ

CMDTARGET = pmdamailq$(EXECSUFFIX)
CFILES	= mailq.c
DFILES	= README
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_REGEX)
LDIRT	= domain.h *.log *.dir *.pag so_locations

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(CMDTARGET) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root help pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
