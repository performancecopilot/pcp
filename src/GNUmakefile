#
# Copyright (c) 2012-2018 Red Hat.
# Copyright (c) 2000,2004,2012 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

INCLUDE_SUBDIR = include
PMNS_SUBDIR = pmns
LIBPCP_SUBDIR = libpcp libpcp_static
LIBS_SUBDIRS = \
	libpcp_pmda \
	libpcp_trace \
	libpcp_pmcd \
	libpcp_gui \
	libpcp_mmv \
	libpcp_import \
	libpcp_qed \
	libpcp_qmc \
	libpcp_qwt \
	libpcp_web \
	#

OTHER_SUBDIRS = \
	pminfo \
	pmprobe \
	bashrc \
	zshrc \
	dbpmda \
	genpmda \
	newhelp \
	pcp \
	pcp2elasticsearch \
	pcp2graphite \
	pcp2influxdb \
	pcp2json \
	pcp2spark \
	pcp2xlsx \
	pcp2xml \
	pcp2zabbix \
	pmafm \
	pmfind \
	pmcpp \
	pmcd \
	pmcd_wait \
	pmchart \
	pmclient \
	pmconfig \
	pmdas \
	pmdate \
	pmdbg \
	pmdumplog \
	pmdumptext \
	pmerr \
	pmgenmap \
	pmgetopt \
	pmhostname \
	pmie \
	pmieconf \
	pmiestatus \
	pmjson \
	pmlc \
	pmlock \
	pmlogextract \
	pmlogger \
	pmlogreduce \
	pmlogconf \
	pmloglabel \
	pmlogrewrite \
	pmlogsize \
	pmlogsummary \
	pmlogcheck \
	pmmgr \
	pmpost \
	pmproxy \
	pmquery \
	pmrep \
	pmseries \
	pmstat \
	pmstore \
	pmsocks \
	pmtrace \
	pmsignal \
	pmsleep \
	pmsnap \
	pmtime \
	pmval \
	pmview \
	pmwebd \
	perl \
	python \
	procmemstat \
	autofsd-probe \
	telnet-probe \
	collectl2pcp \
	iostat2pcp \
	mrtg2pcp \
	sar2pcp \
	sheet2pcp \
	ganglia2pcp \
	webapps \
	win32ctl \
	zabbix-agent \
	pmpython \
	discover \
	derived \
	selinux \
	zeroconf \
	#

SUBDIRS = \
	$(INCLUDE_SUBDIR) \
	$(LIBPCP_SUBDIR) \
	$(PMNS_SUBDIR) \
	$(LIBS_SUBDIRS) \
	$(OTHER_SUBDIRS)

default :: default_pcp

include $(BUILDRULES)

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

$(LIBPCP_SUBDIR): $(INCLUDE_SUBDIR)
$(PMNS_SUBDIR): $(LIBPCP_SUBDIR)
$(LIBS_SUBDIRS): $(PMNS_SUBDIR)
$(OTHER_SUBDIRS): $(LIBS_SUBDIRS)
