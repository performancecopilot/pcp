#
# Original implementation by Troy Dawson (dawson@fnal.gov)
# 
# Copyright (c) 2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015 Red Hat.
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

IAM	= lmsensors
DOMAIN	= LMSENSORS
PYSCRIPT = pmda$(IAM).python
SCRIPTS	= Install Remove $(PYSCRIPT)
DFILES	= README pmns help root

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h $(IAM).log help.pag help.dir

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default ::  build-me
install_pcp : install

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me: check_domain

install :: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) $(PMDADIR)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

check_domain:   ../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^

