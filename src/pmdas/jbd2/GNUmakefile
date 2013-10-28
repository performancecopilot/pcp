#
# Copyright (c) 2013 Red Hat.
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
include	$(TOPDIR)/src/include/builddefs

IAM		= jbd2
DOMAIN		= JBD2
CMDTARGET	= pmdajbd2
LIBTARGET	= pmda_jbd2.so
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "jbd2	122	dso	jbd2_init	$(PMDADIR)/$(LIBTARGET)"

CFILES		= proc_jbd2.c pmda.c
HFILES		= proc_jbd2.h convert.h
SCRIPTS		= Install Remove
LSRCFILES 	= help root root_jbd2 $(SCRIPTS)
LDIRT		= help.dir help.pag domain.h

LLDLIBS		= $(PCP_PMDALIB)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag root root_jbd2 $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 root_jbd2 $(PCP_VAR_DIR)/pmns/root_jbd2
else
build-me:
install:
endif

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_jbd2 -v 2 -o help < help

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmda.o:			domain.h
pmda.o proc_jbd2.o:	convert.h
pmda.o proc_jbd2.o:	proc_jbd2.h
