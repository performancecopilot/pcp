# 
# Copyright (c) 2021 Red Hat.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# See <http://www.gnu.org/licenses/> for copy of the GNU General Public License.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= hdb
PYSCRIPT = pmda$(IAM).python
LDIRT	= domain.h root pmns $(IAM).log
DOMAIN	= HDB

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
CONFIGDIR  = $(PCP_SYSCONF_DIR)/$(IAM)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me:	check_domain
install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(CONFIGDIR)
	$(INSTALL) -m 600 -t $(PMDATMPDIR)/$(IAM).conf $(IAM).conf $(CONFIGDIR)/$(IAM).conf
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/README.md README.md $(PMDAADMDIR)/README.md
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^

check:: $(MAN_PAGES)
	$(MANLINT) $^

test:
	@ln -s pmdahdb.python pmdahdb.py
	@./pmdahdb_test.python
	@rm pmdahdb.py
