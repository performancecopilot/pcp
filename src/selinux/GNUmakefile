#
# Copyright (c) 2017-2018,2020-2022 Red Hat.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
IAM = pcp

LDIRT = $(IAM).cil $(IAM).mod $(IAM).pp $(IAM).pp.bz2
LDIRDIRT = tmp

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_SELINUX)" "true"
build-me: $(IAM).pp.bz2

PACKAGES_SELINUX_DIR = $(PCP_SELINUX_DIR)/packages
TARGETED_SELINUX_DIR = $(PCP_SELINUX_DIR)/packages/targeted

install: default
	$(INSTALL) -m 755 -d $(PCP_SELINUX_DIR)
	$(INSTALL) -m 755 -d $(PACKAGES_SELINUX_DIR)
	$(INSTALL) -m 755 -d $(TARGETED_SELINUX_DIR)
	$(INSTALL) -m 644 $(IAM).pp.bz2 $(TARGETED_SELINUX_DIR)/$(IAM).pp.bz2
else
build-me:
install:
endif

default_pcp: default

install_pcp : install

$(IAM).pp: $(IAM).te $(IAM).fc $(IAM).if
	# lots of duplicate definition from system container.if - filter out
	$(MAKE) -f /usr/share/selinux/devel/Makefile $(IAM).pp 2>&1 | grep -v container.if

$(IAM).pp.bz2: $(IAM).pp
	$(BZIP2) -9 < $^ > $@

