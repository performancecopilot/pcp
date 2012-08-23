#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

SUBDIRS = include \
	  libpcp libpcp_pmda libpcp_trace libpcp_http libpcp_pmcd libpcp_gui \
	  libpcp_mmv \
	  pmconfig pmns newhelp pminfo pmcpp \
	  libpcp_import \
	  pmcd pmerr pmdbg \
	  pmcdctl pmlogctl win32ctl \
	  pmdate pmie pmieconf pmpost pmprobe pmlock pmval pmlogger \
	  pmdumplog pmlogextract pmstore pmhostname pmgenmap \
	  pmlogconf pmloglabel pmlogsummary pmclient pcp pmlc dbpmda \
	  pmtrace pmstat pmsocks pmdas pmafm procmemstat pmimport \
	  pmlogreduce genpmda pmproxy pmsignal pmsleep pmtop python \
	  autofsd-probe telnet-probe bashrc pmiestatus \
	  pmevent pmlogrewrite

ifneq "$(TARGET_OS)" "mingw"
SUBDIRS	+= cpan
endif

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
