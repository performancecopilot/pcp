#!gmake
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

# configure me please!
MINGW = /cygdrive/c/mingw
#MINGW = /mingw
SHIM_CC	= $(MINGW)/bin/gcc.exe

IAM		= windows
DOMAIN		= WINDOWS
CFILES		= pmda.c data.c
HFILES		= shm.h shim_pcp.h shim.h
SHIM_CFILES	= shim.c pdherr.c util.c init.c instance.c help.c fetch.c
SHIM_OBJECTS	= $(SHIM_CFILES:.c=.o)
CYGWIN_CFILES	= pmcygwinserver.c
CYGWIN_OBJECTS	= $(CYGWIN_CFILES:.c=.o)
LLDLIBS		= -lpcp -lpcp_pmda
LCFLAGS		= -I. -DWIN32_LEAN_AND_MEAN=1
PMNS		= pmns.disk pmns.kernel pmns.mem pmns.network \
		  pmns.sqlserver pmns.filesys pmns.hinv pmns.pmda
LSRCFILES	= $(SHIM_CFILES) $(CYGWIN_CFILES) pmcygrunsrv.sh \
		  Install Remove $(PMNS) root README GNUmakefile.install \
		  libpdh.def pdhlist.c pdhmatch.sh
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET	= pmdawindows.exe
SHIMTARGET	= shim.exe
EXTRATARGETS	= pmcygwinserver.exe pdhlist.exe

LDIRT		= domain.h *.o $(IAM).log pmda$(IAM) libpdh.a pdhlist.exe \
		  platform_defs.h shim.exe pmcygwinserver.exe pcp.windows.shm

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "cygwin"
build-me: $(LSRCFILES) $(CMDTARGET) $(SHIMTARGET) $(EXTRATARGETS)
install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 755 shim.exe $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 755 pdhlist.exe $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 README root $(PMNS) domain.h $(CFILES) $(PMDADIR)
	$(INSTALL) -m 755 -d $(PCP_BIN_DIR)
	$(INSTALL) -m 755 pmcygrunsrv.sh $(PCP_BIN_DIR)/pmcygrunsrv
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/bin
	$(INSTALL) -m 755 pmcygwinserver.exe $(PCP_SHARE_DIR)/bin/pmcygwinserver.exe
else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

pmcygwinserver.exe:	$(CYGWIN_OBJECTS)
	$(CCF) -o pmcygwinserver.exe $(LDFLAGS) $(CYGWIN_OBJECTS)

platform_defs.h:	$(TOPDIR)/src/include/platform_defs.h
	echo "/* special Win32 API quickie hacks */" >platform_defs.h
	echo "typedef int __int32_t;" >>platform_defs.h
	echo "typedef unsigned int __uint32_t;" >>platform_defs.h
	echo "" >>platform_defs.h
	sed <$(TOPDIR)/src/include/platform_defs.h >>platform_defs.h \
	    -e '/sys\/param.h/d'

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

$(OBJECTS):	shm.h domain.h

pmdawindows:	$(OBJECTS)
	$(CCF) -o pmdawindows.exe $(LDFLAGS) $(OBJECTS) $(LDLIBS)

libpdh.a:	libpdh.def
	$(DLLTOOL) -d libpdh.def -k -l libpdh.a

shim.exe:	shm.h shim.h shim_pcp.h domain.h $(SHIM_OBJECTS) libpdh.a
	$(SHIM_CC) $(CFLAGS) -o shim.exe $(SHIM_OBJECTS) libpdh.a

PDHLIST_OBJECTS = pdhlist.o pdherr.o
pdhlist.exe:	$(PDHLIST_OBJECTS) libpdh.a
	$(SHIM_CC) -o pdhlist.exe $(PDHLIST_OBJECTS) libpdh.a

$(SHIM_OBJECTS) pdhlist.o:
	$(SHIM_CC) $(CFLAGS) -c -o $@ `echo $@ | sed -e 's/\.o/.c/g'`
