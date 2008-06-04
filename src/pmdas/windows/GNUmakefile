#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

IAM		= windows
DOMAIN		= WINDOWS
CFILES		= pmda.c error.c open.c instance.c fetch.c help.c
HFILES		= hypnotoad.h
LCFLAGS		= -DWIN32_LEAN_AND_MEAN
LLDLIBS		= -lpcp -lpcp_pmda libpdh.a
PMNS		= pmns.disk pmns.kernel pmns.mem pmns.network \
		  pmns.sqlserver pmns.filesys pmns.hinv pmns.pmda \
		  pmns.process
LSRCFILES	= Install Remove $(PMNS) root README GNUmakefile.install \
		  libpdh.def pdhlist.c pdhmatch.sh
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET	= pmdawindows.exe
LIBTARGET	= pmda_windows.dll
EXTRATARGETS	= pdhlist.exe

LDIRT		= root_windows domain.h $(IAM).log pmda$(IAM) libpdh.a \
		  $(EXTRATARGETS) pdhlist.o

CONF_LINE = "windows	79	dso	windows_init	$(PCP_PMDAS_DIR)/windows/pmda_windows.dll"

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "mingw"
build-me: root_windows $(LSRCFILES) $(CMDTARGET) $(LIBTARGET) $(EXTRATARGETS)
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 755 pdhlist.exe $(CMDTARGET) $(LIBTARGET) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 README root $(PMNS) domain.h $(CFILES) $(PMDADIR)
	$(INSTALL) -m 644 root_windows $(PCP_VAR_DIR)/pmns/root_windows
else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

root_windows:	../../pmns/stdpmid $(PMNS)
	rm -f root_windows
	cpp -I../../pmns -fno-show-column < root | sed -e '/^#/d' > root_windows

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

$(OBJECTS):	hypnotoad.h domain.h libpdh.a

libpdh.a:	libpdh.def
	$(DLLTOOL) -d libpdh.def -k -l libpdh.a

pdhlist.exe:	pdhlist.o error.o
	$(CCF) -o pdhlist.exe pdhlist.o error.o libpdh.a
