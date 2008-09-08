#! gmake
#
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
# $Id: GNUmakefile,v 1.1 2007/09/11 01:38:10 kimbrr Exp $

TOPDIR = ../../../..
include	$(TOPDIR)/src/include/builddefs

HFILES = Bool.h	 Desc.h Hash.h List.h PMC.h String.h Context.h \
	 Group.h Indom.h Metric.h Source.h Vector.h
LDIRT = pcp

default: pcp

pcp:
	$(LN_S) $(TOPDIR)/src/include pcp

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PCP_INC_DIR)/pmc
	$(INSTALL) -m 644 $(HFILES) $(PCP_INC_DIR)/pmc

default_pcp:	default

install_pcp:	install
