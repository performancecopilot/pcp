#
# Copyright (c) 2018,2020 Red Hat.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM       = bcc
PYSCRIPTS = $(shell echo *.python)
EBPFPROGS = $(shell echo *.bpf)
SUBDIRS  = fs
MODULETMP = $(PCP_PMDAS_DIR)/$(IAM)/modules
MODULEDIR = $(PCP_PMDASADM_DIR)/$(IAM)/modules

include $(BUILDRULES)

default_pcp default:	$(SCRIPTS) $(BCC_EBPF) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install:	default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(MODULEDIR)
	$(INSTALL) -m 755 -d $(MODULETMP)
	$(INSTALL) -m 644 -t $(MODULETMP) $(EBPFPROGS) $(PYSCRIPTS) $(MODULEDIR)

check:: $(PYSCRIPTS)
	$(PYLINT) $^
