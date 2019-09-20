#
# Copyright (c) 2019 Red Hat.
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

IAM		= netcheck
PYSCRIPT	= pmda$(IAM).python
CONF		= $(IAM).conf
LDIRT		= domain.h root pmns $(IAM).log

DOMAIN		= NETCHECK
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
SUBDIRS		= modules

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default ::	build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me: check_domain

install_pcp install ::	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove pyprep $(PYSCRIPT) $(PMDADIR)
	$(INSTALL) -m 644 pmdautil.python $(PMDADIR)/pmdautil.python
	$(INSTALL) -m 644 $(CONF) $(PMDADIR)/$(CONF)
	@$(INSTALL_MAN)

default_pcp default ::	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install ::	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^
