#! gmake
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
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
# $Id: GNUmakefile,v 2.165 2006/06/30 05:47:11 makc Exp $
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

# directories that are only used for IRIX builds, but need to be descended
# into for clean and clobber for shared build areas
#
IRIX_SUBDIRS = buildtools libarray_util libpcp_dev

# directories that are only used for open source builds
#
GNU_SUBDIRS = libkmtime

SUBDIRS = $(IRIX_SUBDIRS) \
	  include libpcp pmns libpcp_pmda libpcp_trace libpcp_http \
	  $(GNU_SUBDIRS) newhelp pminfo pmnscomp pmcd pmerr pmdbg \
	  pmdate pmie pmpost pmprobe pmcdctl pmlock pmval pmlogger \
	  pmdumplog pmlogextract pmstore pmhostname pmgenmap pmlogctl \
	  pmlogconf pmlogsummary pmclient pmkstat pcp pmlc dbpmda \
	  xconfirm pmtrace pmstat pmsocks pmdas pmafm procmemstat \
	  pmlogreduce genpmda pmproxy telnet-probe pmsleep

ifneq ($(TARGET_OS), cygwin)
SUBDIRS += libpcp_pmc pmdumptext autofsd-probe
endif

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
