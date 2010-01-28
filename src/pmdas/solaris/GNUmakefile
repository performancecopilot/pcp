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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= solaris
DOMAIN	= SOLARIS
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET = pmdasolaris
LIBTARGET = pmda_solaris.so
CONF_LINE = "solaris	75	dso	solaris_init	$(PMDADIR)/$(LIBTARGET)"

CFILES	= solaris.c data.c sysinfo.c disk.c zpool.c zfs.c \
	 zpool_perdisk.c netmib2.c netlink.c

BARE_NS = disk kernel mem network hinv zpool zfs zpool_perdisk
PMNS	= $(BARE_NS:%=pmns.%)
LSRCFILES = $(PMNS) help root common.h
LDIRT	= domain.h *.log *.dir *.pag root_solaris

LLDLIBS = -lpcp -lpcp_pmda -lkstat -lzfs -lnvpair

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "solaris" 
build-me: root_solaris domain.h $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_solaris $(PCP_VAR_DIR)/pmns/root_solaris
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(OBJECTS):	common.h

help.dir help.pag: help root_solaris
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_solaris -v 2 -o help < help

root_solaris: ../../pmns/stdpmid $(PMNS)
	rm -f root_solaris
	$(CPP_SIMPLE) -I../../pmns < root | sed -e '/^#/d' > root_solaris

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'
