#
# Copyright (C) 2000 Michal Kara.  All Rights Reserved.
# Copyright (c) 2015 Red Hat.
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

IAM	= apache
DOMAIN	= APACHE
TARGETS	= $(IAM)$(EXECSUFFIX)
CFILES	= apache.c
SCRIPTS	= Install Remove
DFILES	= README
LSRCFILES= $(SCRIPTS) pmns help root Apache.pmchart $(DFILES) \
	pmlogconf.summary pmlogconf.processes pmlogconf.uptime

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS)
LCFLAGS = -I$(TOPDIR)/src/libpcp_http/src
LLDFLAGS= -L$(TOPDIR)/src/libpcp_http/src
LLDLIBS	= -lpcp_http $(PCP_PMDALIB)

default: build-me

include $(BUILDRULES)

build-me: $(TARGETS)

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
	$(INSTALL) -m 644 Apache.pmchart $(PMCHART)/Apache
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
	$(INSTALL) -m 644 pmlogconf.summary $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)/summary
	$(INSTALL) -m 644 pmlogconf.processes $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)/processes
	$(INSTALL) -m 644 pmlogconf.uptime $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)/uptime

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp : default

install_pcp : install
