#
# Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020 Red Hat.
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
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

IAM	= overhead
DOMAIN	= OVERHEAD
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)
HFILES	= overhead.h
CFILES	= overhead.c pmda.c refresh.c config.c
CONFIG	= sample.conf default.conf
OTHERS	= README root pmns
LDIRT	= *.log *.dir *.pag so_locations a.out domain.h $(CMDTARGET)
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

default:	build-me

include $(TOPDIR)/src/include/buildrules

ifeq "$(TARGET_OS)" "linux"
build-me:	$(CMDTARGET)

install: build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(CMDTARGET) $(CMDTARGET) $(PMDAADMDIR)/$(CMDTARGET) 
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(OTHERS) domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 755 -d $(PMDACONFIG)/conf.d
	$(INSTALL) -m 755 -d $(PMDACONFIG)/examples
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/sample.conf sample.conf $(PMDACONFIG)/examples/sample.conf
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/default.conf default.conf $(PMDACONFIG)/conf.d/default.conf
	$(INSTALL) -m 755 overhead.pmcheck $(PCP_SHARE_DIR)/lib/pmcheck/pmda-overhead
else
build-me:
install: $(TOPDIR)/src/pmcheck/pmda.na.template
	$(INSTALL) -m 755 $< $(PCP_SHARE_DIR)/lib/pmcheck/pmda-overhead
endif

$(CMDTARGET):	$(OBJECTS)

$(OBJECTS): domain.h overhead.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install

config: config.o
	cc -g -o config config.o -lpcp
