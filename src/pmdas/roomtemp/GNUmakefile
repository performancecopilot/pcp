#!gmake
#
# Copyright (c) 2000-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

HFILES = dsread.h
CFILES	= roomtemp.c dsread.c
HFILES	= dsread.h
CMDTARGET = pmdaroomtemp
LLDFLAGS = -L.
LLDLIBS = -lmlan -lpcp -lpcp_pmda
LCFLAGS = -I.
DFILES	= README help
LSRCFILES = Install Remove pmns $(DFILES) root GNUmakefile.install

SUBDIRS = mlan

IAM	= roomtemp
DOMAIN	= ROOMTEMP
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h pcp $(IAM).log pmda$(IAM) pmda_$(IAM).so libmlan.a

default: build-me

include $(BUILDRULES)

ifeq "$(findstring $(TARGET_OS),solaris linux)" ""
build-me:
	$(SUBDIRS_MAKERULE)

install:
else
build-me: pcp domain.h libmlan.a $(CMDTARGET)

install: default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
endif

domain.h: ../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

libmlan.a : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	rm -f libmlan.a
	ln -sf mlan/libmlan.a .

pcp:
	$(LN_S) -f $(TOPDIR)/src/include pcp

default_pcp : default

install_pcp : install
