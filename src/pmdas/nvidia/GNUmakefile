#
# Copyright (c) 2014 Red Hat.
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

IAM	= nvidia
DOMAIN	= NVML

CMDTARGET = pmdanvidia$(EXECSUFFIX)
CFILES	= localnvml.c nvidia.c
HFILES	= localnvml.h
LSRCFILES = Install Remove root help pmns
LLDLIBS	= $(PCP_PMDALIB)

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
LDIRT	= domain.h *.log *.dir *.pag so_locations

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/$(CMDTARGET)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 root help pmns domain.h $(PMDADIR)

nvidia.o:	domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
