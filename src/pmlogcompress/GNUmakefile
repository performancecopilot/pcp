#
# Copyright (c) 2020 Red Hat, Inc.  All Rights Reserved.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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

TARGETS = pmlogcompress pmlogdecompress

default :: default_pcp

default_pcp : $(TARGETS)

install :: install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 755 pmlogcompress $(PCP_BIN_DIR)/pmlogcompress$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogdecompress $(PCP_BIN_DIR)/pmlogdecompress$(SHELLSUFFIX)

include $(BUILDRULES)
