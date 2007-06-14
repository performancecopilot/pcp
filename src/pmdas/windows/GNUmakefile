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
# $Id: GNUmakefile,v 1.11 2007/02/20 00:08:32 kimbrr Exp $
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM		= windows
DOMAIN		= WINDOWS
CFILES		= pmda.c data.c
HFILES		= shm.h shim_pcp.h shim.h
SHIM_CFILES	= shim.c pdherr.c util.c init.c instance.c help.c fetch.c
SHIM_OBJECTS	= $(SHIM_CFILES:.c=.obj)
CYGWIN_CFILES	= pmcygwinserver.c
CYGWIN_OBJECTS	= $(CYGWIN_CFILES:.c=.o)
LLDLIBS		= -lpcp -lpcp_pmda
LCFLAGS		= -I.
PMNS		= pmns.disk pmns.kernel pmns.mem pmns.network \
		  pmns.sqlserver pmns.filesys pmns.hinv pmns.pmda
LSRCFILES	= $(SHIM_CFILES) $(CYGWIN_CFILES) pmcygrunsrv \
		    Install Remove $(PMNS) root README \
		    GNUmakefile.install shim.save \
		    match-counters show-all-ctrs.c show-all-ctrs.save \
		    all-on-tower
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
DDKROOT		= C:\WINDDK\3790
SDKROOT		= C:\Program Files\Microsoft SDK
# correct for Windows 2003 server on Intel 386
# I don't know what the other options are
MSPLATFORM	= wnet
MSARCH		= i386

LDIRT		= domain.h *.o $(IAM).log pmda$(IAM) windows.log data.dat \
		    core.* *.obj *.pdb platform_defs.h pmdbg.h \
		    shim.exe show-all-ctrs.exe \
		    pcp.windows.shm

do_build	= false
ifeq "$(TARGET_OS)" "cygwin"
do_build	= true
endif
ifeq "$(TARGET_OS)" "interix"
do_build	= true
endif

ifeq "$(do_build)" "true"
# really build me, on Cygwin and interix, need the DLL to bridge gcc and Windows
# C compiler gulfs
CMDTARGET	= pmdawindows.exe
SHIMTARGET	= check_shim
CYGWINTARGET	= pmcygwinserver
else
CMDTARGET	=
SHIMTARGET	=
CYGWINTARGET	=
endif

default_pcp default:	$(LSRCFILES) $(CMDTARGET) $(SHIMTARGET) $(CYGWINTARGET)

SHIM_CC		= '$(DDKROOT)\bin\x86\cl'
SHIM_CFLAGS	= -DPCP_DEBUG -DPCP_VERSION=\"$(PCP_VERSION)\" -DWIN32 /Od /Zi /I$(TOPDIR)/src/include /I'$(DDKROOT)\inc\$(MSPLATFORM)' /I'$(DDKROOT)\inc\crt' /I'$(SDKROOT)\include'
SHIM_LINK	= '$(DDKROOT)\bin\x86\link'
SHIM_LINK_FLAGS	= /libpath:'$(DDKROOT)\lib\$(MSPLATFORM)\$(MSARCH)'

check_shim:
	rm -f /tmp/check_cl.*
	echo "int i;" >/tmp/check_cl.c
	if ( cd /tmp; $(SHIM_CC) /c check_cl.c >/dev/null 2>&1 ); then $(MAKE) shim.exe show-all-ctrs.exe; \
	else if [ ! -f shim.exe ]; then cp shim.save shim.exe; fi; \
	     if [ ! -f show-all-ctrs.exe ]; then cp show-all-ctrs.save show-all-ctrs.exe; fi; \
	fi

ifeq "$(do_build)" "true"
save:	shim.exe show-all-ctrs.exe
	cp shim.exe shim.save
	cp show-all-ctrs.exe show-all-ctrs.save
endif

.SUFFIXES:	.obj
.c.obj:
	$(SHIM_CC) $(SHIM_CFLAGS) /c /Fo$@ $<
.cpp.obj:
	$(SHIM_CC) $(SHIM_CFLAGS) /c /Fo$@ $<

include $(BUILDRULES)

ifeq "$(do_build)" "true"
install install_pcp:	check_shim default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 755 shim.exe $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 755 show-all-ctrs.exe $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 README root $(PMNS) domain.h $(CFILES) $(PMDADIR)
	$(INSTALL) -m 755 -d $(PCP_BIN_DIR)
	$(INSTALL) -m 755 pmcygrunsrv $(PCP_BIN_DIR)/pmcygrunsrv
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/bin
	$(INSTALL) -m 755 $(CYGWINTARGET) $(PCP_SHARE_DIR)/bin/$(CYGWINTARGET)
else
install install_pcp:
endif

pmcygwinserver.exe:	$(CYGWIN_OBJECTS)
	$(CCF) -o pmcygwinserver.exe $(LDFLAGS) $(CYGWIN_OBJECTS)

shim.exe:	platform_defs.h $(SHIM_OBJECTS)
	rm -f shim.exe
	$(SHIM_LINK) /out:shim.exe $(SHIM_OBJECTS) user32.lib pdh.lib advapi32.lib $(SHIM_LINK_FLAGS)

$(SHIM_OBJECTS):	platform_defs.h shm.h shim.h shim_pcp.h domain.h
util.obj:		pmdbg.h

platform_defs.h:	$(TOPDIR)/src/include/platform_defs.h
	echo "/* special Win32 API quickie hacks */" >platform_defs.h
	echo "typedef int __int32_t;" >>platform_defs.h
	echo "typedef unsigned int __uint32_t;" >>platform_defs.h
	echo "" >>platform_defs.h
	sed <$(TOPDIR)/src/include/platform_defs.h >>platform_defs.h \
	    -e '/sys\/param.h/d'

pmdbg.h:
	cp $(TOPDIR)/src/include/pmdbg.h pmdbg.h

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <../../pmns/stdpmid >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

$(OBJECTS):		shm.h domain.h
pmdawindows:		$(OBJECTS)
	$(CCF) -o pmdawindows $(LDFLAGS) $(OBJECTS) $(LDLIBS)

foo:
	@echo PATH=$(PATH)
	@echo do_build=$(do_build)
	@echo LSRCFILES=$(LSRCFILES)
	@echo CMDTARGET=$(CMDTARGET)
	@echo SHIMTARGET=$(SHIMTARGET) 

show-all-ctrs.exe:	show-all-ctrs.obj pdherr.obj
	$(SHIM_LINK) /out:show-all-ctrs.exe show-all-ctrs.obj pdherr.obj pdh.lib advapi32.lib $(SHIM_LINK_FLAGS)

uuidgen.exe:	uuidgen.obj
	$(SHIM_LINK) /out:uuidgen.exe uuidgen.obj advapi32.lib $(SHIM_LINK_FLAGS)

win_test.exe:	win_test.obj
	$(SHIM_LINK) /out:win_test.exe win_test.obj advapi32.lib $(SHIM_LINK_FLAGS)
