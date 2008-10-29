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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

HFILES = pmapi.h impl.h pmda.h trace.h trace_dev.h trace_hash.h
GENERATED_HFILES = pmdbg.h platform_defs.h
CONFFILES = platform_defs.h builddefs pcp.conf

LSRCFILES = platform_defs.h.in builddefs.in buildrules pcp.conf.in \
	    pcp.env mk_pmdbg trace_dev.h trace_hash.h 

LDIRT = $(GENERATED_HFILES) builddefs.install include

default :: default_pcp

default_pcp : pcp.conf $(HEADERS) $(GENERATED_HFILES) builddefs.install

include $(BUILDRULES)

install :: default_pcp install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 644 pcp.conf /etc/pcp.conf
	$(INSTALL) -m 644 pcp.env /etc/pcp.env
	$(INSTALL) -m 644 $(HFILES) $(GENERATED_HFILES) buildrules $(PCP_INC_DIR)
	$(INSTALL) -m 644 builddefs.install $(PCP_INC_DIR)/builddefs

pmdbg.h : pmapi.h
	./mk_pmdbg

builddefs.install : builddefs
	sed -e 's;^include .*pcp.conf;include /etc/pcp.conf;' <builddefs >builddefs.install
