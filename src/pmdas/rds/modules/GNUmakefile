#
# Copyright (c) 2025 Oracle and/or its affiliates
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
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

IAM		= rds
PYSCRIPTS	= $(shell echo *.python)

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)/modules
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)/modules

include $(BUILDRULES)

default_pcp default:	$(SCRIPTS)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(PYSCRIPTS) $(PMDAADMDIR)

check:: $(PYSCRIPTS)
	$(PYLINT) $^
