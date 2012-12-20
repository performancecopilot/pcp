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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= cisco
DOMAIN	= CISCO
TARGETS	= $(IAM)$(EXECSUFFIX) probe$(EXECSUFFIX) parse$(EXECSUFFIX)
CFILES	= cisco.c pmda.c interface.c telnet.c
HFILES	= cisco.h 
DFILES	= README
LSRCFILES=Install Remove root pmns Cisco.pmchart help Samples Tested $(DFILES) \
	cisco.in_util.pmie cisco.out_util.pmie probe.c

LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_PTHREADS)

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart
PMIE	= $(PCP_SYSCONF_DIR)/pmie/$(IAM)

LDIRT	= domain.h cisco.log *.dir *.pag so_locations a.out parse.? probe.o \
	$(TARGETS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM) 
	$(INSTALL) -m 755 probe parse Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 Cisco.pmchart $(PMCHART)/Cisco
	$(INSTALL) -m 755 -d $(PMIE)
	$(INSTALL) -m 644 cisco.in_util.pmie $(PMIE)/in_util
	$(INSTALL) -m 644 cisco.out_util.pmie $(PMIE)/out_util
else
build-me:
install:
endif

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

parse$(EXECSUFFIX):	parse.o cisco.o interface.o telnet.o
	$(CCF) -o $@ $(LDFLAGS) parse.o cisco.o interface.o telnet.o $(LDLIBS)

parse.c:	pmda.c
	ln -s pmda.c parse.c

parse.o:	parse.c
	$(CCF) -c -DPARSE_ONLY parse.c

probe$(EXECSUFFIX):	probe.o interface.o telnet.o
	$(CCF) -o $@ $(LDFLAGS) probe.o interface.o telnet.o $(LDLIBS)

cisco.o:	domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
