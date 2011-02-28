#!gmake
#
# Copyright (c) 2011, Nathan Scott.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
WINDIR = $(TOPDIR)/src/win32ctl

IAM		= etw
DOMAIN		= ETW
CFILES		= 
HFILES		= 
LSRCFILES	= tdhlist.c
LCFLAGS		= -I$(WINDIR)/include -D_WIN32_WINNT=0x0600
LLDFLAGS	= -L$(WINDIR)/lib
LLDLIBS		= -lpcp_tdh

PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM) 
#CMDTARGET	= pmdaetw.exe
#LIBTARGET	= pmda_etw.dll
EXTRATARGETS	= tdhlist.exe

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "mingw"
build-me: $(LSRCFILES) $(CMDTARGET) $(LIBTARGET) $(EXTRATARGETS)
install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 tdhlist.exe $(PMDADIR)/tdhlist.exe
else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

tdhlist.exe:	tdhlist.o
	$(CCF) -o tdhlist.exe tdhlist.o $(LLDFLAGS) -lpcp_tdh
