#
# Copyright (c) 2014-2015,2018 Red Hat.
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

IAM	= json
PYSCRIPT = pmda$(IAM).python
LCFILES = acme_json.c
LTARGET = acme_json$(EXECSUFFIX)
SCRIPTS = Install Remove generate_ceph_metadata

DOMAIN	= JSON
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
DEMODIR = $(PCP_DEMOS_DIR)/$(IAM)
DEMOFILES = README.demos Makefile.demos

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LLDFLAGS = -L$(TOPDIR)/src/libpcp/src
LLDLIBS = $(PCPLIB)
LDIRT	= domain.h $(IAM).log acme_json

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_JSON)" "true"
build-me:	check_domain config.json
install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_TMP_DIR)/$(IAM)
	$(INSTALL) -m 755 $(SCRIPTS) $(PYSCRIPT) $(PMDADIR)
	$(INSTALL) -m 644 config.json README $(PMDADIR)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 Makefile.demos $(DEMODIR)/Makefile
	$(INSTALL) -m 644 README.demos $(DEMODIR)/README
	$(INSTALL) -m 644 acme_json.c $(DEMODIR)/acme_json.c
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

config.json:	config.json.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_TMP_DIR@;'$(PCP_TMP_DIR)';' \
	# END

# check-build only, binary not installed (but source is)
$(LTARGET):	acme_json.c
	$(CCF) -o $@ $^ $(LDFLAGS) $(LDLIBS)
