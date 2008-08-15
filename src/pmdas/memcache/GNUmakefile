#!gmake
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= memcache
DOMAIN	= MEMCACHE
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LSRCFILES = Install Remove pmdamemcache.pl README

default:

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove pmdamemcache.pl $(PMDADIR)

default_pcp : default

install_pcp : install
