#
# Copyright (c) 2012-2013 Red Hat.
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

SUBDIRS = include libpcp pmns \
	  libpcp_pmda libpcp_trace libpcp_http libpcp_pmcd libpcp_gui \
	  libpcp_mmv libpcp_import \
	  pmconfig newhelp pminfo pmcpp pmcd pmcd_wait pmerr pmdbg \
	  pmdate pmie pmieconf pmpost pmprobe pmlock pmval pmlogger \
	  pmdumplog pmlogextract pmstore pmhostname pmgenmap pmevent \
	  pmlogconf pmloglabel pmlogsummary pmclient pcp pmlc dbpmda \
	  pmtrace pmstat pmsocks pmdas pmafm procmemstat pmimport \
	  pmlogreduce genpmda pmproxy pmsignal pmsleep pmtop \
	  autofsd-probe telnet-probe bashrc pmiestatus \
	  pmevent pmlogrewrite pmcollectl pmatop pmwebapi \
	  perl python win32ctl

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
