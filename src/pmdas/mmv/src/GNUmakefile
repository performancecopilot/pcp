#
# Copyright (c) 2013 Red Hat.
# Copyright (c) 2009-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2000-2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM		= mmv
DOMAIN		= MMV
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
TARGETS		= $(CMDTARGET) $(LIBTARGET)

CFILES		= mmv.c
LSRCFILES	= Install Remove root_mmv
LLDLIBS		= $(PCP_PMDALIB)
LDIRT		= domain.h *.log *.dir *.pag pmns

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE = "mmv	70	dso	mmv_init	$(PCP_PMDAS_DIR)/mmv/pmda_mmv.$(DSOSUFFIX)"

default_pcp default: $(TARGETS) pmns
	@if [ `grep -c $(CONF_LINE) ../../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../../pmcd.conf ; \
	fi

include $(BUILDRULES)

install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(TARGETS) Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmns $(PMDADIR)/root_mmv
	$(INSTALL) -m 644 root_mmv $(PCP_VAR_DIR)/pmns/root_mmv
ifeq "$(findstring $(PCP_TMP_DIR),/var/tmp /tmp)" ""
	$(INSTALL) -m 1777 -d $(PCP_TMP_DIR)/mmv
endif

$(CMDTARGET):	$(OBJECTS)

$(IAM).o : domain.h

domain.h: ../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmns :
	$(LN_S) -f root_mmv pmns
