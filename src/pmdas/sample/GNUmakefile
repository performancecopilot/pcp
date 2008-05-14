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
#*************************************************************************
#
# Makefile - sample PMDA
#
#*************************************************************************

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

DOMAIN  = SAMPLE

DFILES	= README help
LSRCFILES = root pmns $(DFILES) GNUmakefile.install Install Remove \
	    Sample.pmchart
LDIRT = domain.h *.dir *.pag sample.log

SUBDIRS = src
PMDADIR = $(PCP_PMDAS_DIR)/sample

default_pcp default :: domain.h $(SUBDIRS)

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: default
	$(INSTALL) -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns root domain.h $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 Sample.pmchart $(PCP_VAR_DIR)/config/pmchart/Sample

include $(BUILDRULES)

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

domain.h: ../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from pmns/stdpmid by Makefile on `date`" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'
