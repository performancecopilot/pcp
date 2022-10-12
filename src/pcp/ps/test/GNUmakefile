#
# Copyright (c) 2022 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
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

SCRIPT = pcp-ps.py
MODULE = pcp_ps.py

LDIRT = $(MODULE)

default default_pcp build-me install install_pcp:

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
test:
	@rm -f $(MODULE)
	$(LN_S) ../$(SCRIPT) $(MODULE)
	pmpython -m unittest discover -s . -p '*_test.py'
endif
