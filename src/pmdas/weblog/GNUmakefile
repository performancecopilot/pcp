#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= weblog
DOMAIN	= WEBSERVER
TARGETS	= $(IAM)$(EXECSUFFIX) check_match$(EXECSUFFIX)
CFILES	= weblog.c pmda.c sproc.c
HFILES	= weblog.h
SCRIPTS	= Install Remove server.sh weblogconv.sh
CHARTS	= Web.Alarms.pmchart Web.Requests.pmchart Web.Volume.pmchart \
	  Web.Allservers.pmchart Web.Perserver.Bytes.pmchart \
	  Web.Perserver.Requests.pmchart
DFILES	= README
LSRCFILES = pmns help $(DFILES) root $(SCRIPTS) check_match.c $(CHARTS)

LDIRT	= domain.h $(TARGETS) check_match.o

PMDADIR	= $(PCP_PMDAS_DIR)/weblog
PMCHARTDIR = $(PCP_VAR_DIR)/config/pmchart
CONFDIR	= $(PCP_VAR_DIR)/config/web

LDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)

install:	build-me
	# $(INSTALL) -d $(CONFDIR)
	# $(INSTALL) -m 644 weblog.conf $(CONFDIR)/weblog.conf
	$(INSTALL) -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 check_match $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 Web.Alarms.pmchart $(PMCHARTDIR)/Web.Alarms
	$(INSTALL) -m 644 Web.Requests.pmchart $(PMCHARTDIR)/Web.Requests
	$(INSTALL) -m 644 Web.Volume.pmchart $(PMCHARTDIR)/Web.Volume
	$(INSTALL) -m 755 Web.Allservers.pmchart $(PMCHARTDIR)/Web.Allservers
	$(INSTALL) -m 755 Web.Perserver.Bytes.pmchart $(PMCHARTDIR)/Web.Perserver.Bytes
	$(INSTALL) -m 755 Web.Perserver.Requests.pmchart $(PMCHARTDIR)/Web.Perserver.Requests
else
build-me:
install:
endif

weblog$(EXECSUFFIX):	$(OBJECTS)

$(OBJECTS): domain.h

check_match$(EXECSUFFIX):	check_match.o
	$(CCF) -o $@ $(LDFLAGS) check_match.o $(LDLIBS)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
