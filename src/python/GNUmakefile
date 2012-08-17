#
# Copyright (c) 2012 Red Hat.
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
prefix = @prefix@

CFILES = pmapi.c
PYFILES = pcpi.py pcp.py pm-collectl.py setup.py
LSRCFILES = $(PYFILES)

s-setup : $(CFILES) $(PYFILES)
	python setup.py build
	touch s-setup

default: s-setup; @true

include $(BUILDRULES)

# add  --root %(prefix) when prefix is available
install_python: 
	python setup.py install --skip-build

default_pcp:    default

install_pcp install:
