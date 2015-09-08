#
# Copyright (c) 2014-2015 Red Hat.  All Rights Reserved.
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

DFILES	= README help
LSRCFILES = root pmns $(DFILES) Install Remove Sample.pmchart get_next_pmid
LDIRT = *.dir *.pag

SUBDIRS = src
PMDADIR = $(PCP_PMDAS_DIR)/sample

default_pcp default :: $(SUBDIRS)

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: default
	$(INSTALL) -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns root $(PMDADIR)
	$(INSTALL) -m 644 Sample.pmchart $(PCP_VAR_DIR)/config/pmchart/Sample

$(OBJECTS): domain.h

domain.h:	../../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

include $(BUILDRULES)
