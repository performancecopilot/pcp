#
# Copyright (c) 2014-2015,2020 Red Hat.
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
LIBTARGET = pmda_nvidia.$(DSOSUFFIX)
CFILES	= localnvml.c nvidia.c
HFILES	= localnvml.h
DFILES	= README
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_DLOPEN)
LCFLAGS += -DDSOSUFFIX=\"$(DSOSUFFIX)\"
LDIRT	= domain.h *.log *.dir *.pag so_locations

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

default:	$(LIBTARGET) $(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root help pmns domain.h $(PMDAADMDIR)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

default_pcp:	default

install_pcp:	install
