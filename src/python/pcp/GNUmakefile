#
# Copyright (c) 2012-2013,2019 Red Hat.
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

PYCODE = $(shell echo *.py)

LDIRT =

default default_pcp: pmapi.py

include $(BUILDRULES)

install_pcp install:

check:: $(PYCODE)
	$(PYLINT) $^

pmapi.py:	pmapi.py.in fixup
	./fixup
