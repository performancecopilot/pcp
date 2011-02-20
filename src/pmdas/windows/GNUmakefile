#
# Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
WINDIR = $(TOPDIR)/src/win32ctl

IAM		= windows
DOMAIN		= WINDOWS
CFILES		= pmda.c error.c open.c instance.c fetch.c helptext.c
HFILES		= hypnotoad.h libpdh.h
LCFLAGS		= -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0500 \
		  -I$(WINDIR)/include
LLDFLAGS	= -L$(WINDOR)/lib
LLDLIBS		= -lpcp -lpcp_pmda -lpcp_pdh
PMNS		= pmns.disk pmns.kernel pmns.mem pmns.network \
		  pmns.sqlserver pmns.filesys pmns.hinv pmns.pmda \
		  pmns.process
LSRCFILES	= $(PMNS) root help README pdhlist.c pdhmatch.sh
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET	= pmdawindows.exe
LIBTARGET	= pmda_windows.dll
EXTRATARGETS	= pdhlist.exe help.dir help.pag

LDIRT		= root_windows domain.h $(IAM).log pmda$(IAM) \
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
	$(INSTALL) -m 755 pdhlist.exe $(LIBTARGET) $(PMDADIR)
	$(INSTALL) -m 644 README root $(PMNS) $(PMDADIR)
	$(INSTALL) -m 644 domain.h help.dir help.pag help $(PMDADIR)
	$(INSTALL) -m 644 root_windows $(PCP_VAR_DIR)/pmns/root_windows
else
build-me:
install:
endif

help.dir help.pag : help root_windows
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_windows -v 2 -o help < help

default_pcp:	default

install_pcp:	install

root_windows:	../../pmns/stdpmid $(PMNS)
	rm -f root_windows
	cpp -I../../pmns -fno-show-column < root | sed -e '/^#/d' > root_windows

domain.h: ../../pmns/stdpmid
	rm -f domain.h
	@echo "/*" >domain.h
	@echo " * built from $<" >>domain.h
	@echo " */" >>domain.h
	@$(PCP_AWK_PROG) <$< '\
$$1=="#define" && $$2 == "$(DOMAIN)" { print "#define $(DOMAIN) " $$3 >>"$@"; found++ }\
END	{ if (found == 0) { print "Botch: no #define for domain $(DOMAIN) in $<";  exit(1) }\
          if (found > 1) { print "Botch: multiple #defines for domain $(DOMAIN) in $<"; print "... see $@ for details"; exit(1) }\
	  exit(0)\
	}'

$(OBJECTS):	hypnotoad.h domain.h

pdhlist.exe:	pdhlist.o error.o
	$(CCF) -o pdhlist.exe pdhlist.o error.o $(LLDFLAGS) -lpcp_pdh
