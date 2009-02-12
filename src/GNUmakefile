#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

SUBDIRS = include \
	  libpcp libpcp_pmda libpcp_trace libpcp_http libpcp_pmcd libpcp_gui \
	  pmns newhelp pminfo pmnscomp pmcd pmerr pmdbg pmloglabel \
	  pmdate pmie pmpost pmprobe pmcdctl pmlock pmval pmlogger \
	  pmdumplog pmlogextract pmstore pmhostname pmgenmap pmlogctl \
	  pmlogconf pmlogsummary pmclient pcp pmlc dbpmda \
	  xconfirm pmtrace pmstat pmsocks pmdas pmafm procmemstat \
	  pmlogreduce genpmda pmproxy pmsleep pmtop \
	  autofsd-probe telnet-probe cpan win32ctl

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
