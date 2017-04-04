#
# Copyright (c) 2017 Red Hat.
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
-include ./GNUlocaldefs
IAM = pcpupstream

LDIRT = $(IAM).cil $(IAM).pp $(IAM).mod $(IAM).te tmp

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_SELINUX)" "true"
build-me: $(IAM).te

$(IAM).te: $(IAM).te.in
	$(SED) <$< >$@ \
		-e 's;@PCP_CONTAINER_RUNTIME_T@;'$(PCP_CONTAINER_RUNTIME_T)';' \
		-e 's;@PCP_CONTAINER_RUNTIME_RULE@;'$(PCP_CONTAINER_RUNTIME_RULE)';' \
		-e 's;@PCP_NSFS_T@;'$(PCP_NSFS_T)';' \
		-e 's;@PCP_NSFS_RULE@;'$(PCP_NSFS_RULE)';' \

	# END
	make -f /usr/share/selinux/devel/Makefile

install: default
	$(INSTALL) -m 755 -d $(PCP_SELINUX_DIR)
	$(INSTALL) -m 664 $(IAM).pp $(PCP_SELINUX_DIR)/$(IAM).pp
	$(INSTALL) -m 664 $(IAM).te $(PCP_SELINUX_DIR)/$(IAM).te
else
build-me:
install:
endif

default_pcp: default

install_pcp : install
