#
# Copyright (c) 2024 Red Hat.
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

IAM	= amdgpu
DOMAIN	= AMDGPU

CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT = $(IAM)_init

CFILES	= drm.c amdgpu.c
HFILES	= drm.h
DFILES	= README
LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_LIBDRM) $(LIB_FOR_LIBDRM_AMDGPU)
LCFLAGS += -DDSOSUFFIX=\"$(DSOSUFFIX)\" $(CFLAGS_FOR_LIBDRM) $(CFLAGS_FOR_LIBDRM_AMDGPU)
LDIRT	= domain.h *.log *.dir *.pag so_locations

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

ifeq "$(PMDA_AMDGPU)" "true"
default_pcp default:	$(LIBTARGET) $(CMDTARGET)

include $(BUILDRULES)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) root help pmns domain.h $(PMDAADMDIR)
else

default_pcp default:

include $(BUILDRULES)

install_pcp install:

endif

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
