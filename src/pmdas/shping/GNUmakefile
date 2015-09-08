#
# Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= shping
DOMAIN	= SHPING

CMDTARGETS	= $(IAM)$(EXECSUFFIX)
HFILES	= shping.h
CFILES	= shping.c pmda.c
OTHERS	= README root help pmns sample.conf
CHARTS	= shping.CPUTime.pmchart shping.RealTime.pmchart
PMIECS	= shping.status.pmie shping.response.pmie

LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart
PMIE	= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)
LOGCONF	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
LDIRT	= *.log *.dir *.pag so_locations a.out domain.h $(CMDTARGETS)

default:	build-me

include $(TOPDIR)/src/include/buildrules

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(CMDTARGETS)

install: build-me configs
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM) 
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(OTHERS) domain.h $(PMDADIR)
else
build-me:
install: configs
endif

configs:
	$(INSTALL) -m 644 shping.CPUTime.pmchart $(PMCHART)/shping.CPUTime
	$(INSTALL) -m 644 shping.RealTime.pmchart $(PMCHART)/shping.RealTime
	$(INSTALL) -m 755 -d $(PMIE)
	$(INSTALL) -m 644 shping.status.pmie $(PMIE)/status
	$(INSTALL) -m 644 shping.response.pmie $(PMIE)/response
	$(INSTALL) -m 755 -d $(LOGCONF)
	$(INSTALL) -m 644 pmlogconf.summary $(LOGCONF)/summary

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
