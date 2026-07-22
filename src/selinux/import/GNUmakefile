#
# Copyright (c) 2026 Red Hat.
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
IAM = pcp-import

LDIRT = $(IAM).cil $(IAM).mod $(IAM).pp $(IAM).pp.bz2
LDIRDIRT = tmp

TARGETED_SELINUX_DIR = $(PCP_SELINUX_DIR)/packages/targeted

default default_pcp: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_SELINUX)" "true"
build-me: $(IAM).pp.bz2

install install_pcp: default
	$(INSTALL) -m 755 -d $(TARGETED_SELINUX_DIR)
	$(INSTALL) -m 644 $(IAM).pp.bz2 $(TARGETED_SELINUX_DIR)/$(IAM).pp.bz2
else
build-me:
install install_pcp:
endif

$(IAM).pp: $(IAM).te $(IAM).fc $(IAM).if
	@test -d tmp || mkdir tmp
	$(MAKE) -f /usr/share/selinux/devel/Makefile $(IAM).pp

$(IAM).pp.bz2: $(IAM).pp
	$(BZIP2) -9 < $^ > $@
