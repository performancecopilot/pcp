#!gmake
#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM = news
DOMAIN = NEWSAGENT

DFILES = README
LSRCFILES = Install Remove pmns root $(DFILES) active pmda$(IAM).pl

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h

default: domain.h $(TARGETS)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(PMDADIR)/pmda$(IAM).pl
	$(INSTALL) -m 644 $(DFILES) active domain.h pmns root $(PMDADIR)

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(TOPDIR)/src/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]$(DOMAIN)[ 	]/s/.*[ 	]/#define $(DOMAIN) /p' ../../pmns/stdpmid >>domain.h

default_pcp : default

install_pcp : install
