#!gmake
#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

DFILES 	= README
CFILES	= trivial.c
CMDTARGET = pmdatrivial$(EXECSUFFIX)
LLDLIBS = -lpcp -lpcp_pmda
LCFLAGS = -I.
LSRCFILES = Install Remove pmns help $(DFILES) root GNUmakefile.install

IDBTAG	= PCP_DEMO

IAM	= trivial
DOMAIN	= TRIVIAL
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h pcp *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so

default_pcp default:	pcp domain.h $(CMDTARGET)

include $(BUILDRULES)

install install_pcp:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(CFILES) $(PMDADIR)

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

pcp:
	$(LN_S) -f $(TOPDIR)/src/include pcp
