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

ifeq "$(PYTHON_PREFIX)" "/usr"
SETUP_PY_INSTALL_OPTIONS=
else
SETUP_PY_INSTALL_OPTIONS=--prefix=$(PYTHON_PREFIX)
endif

SETUP_PY_INSTALL_OPTIONS += --root=$${DIST_ROOT-/}

CFILES = pmapi.c
PYFILES = pcpi.py pcp.py pm-collectl.py setup.py
LSRCFILES = $(PYFILES)
LDIRDIRT = build
LDIRT = s-setup

s-setup : $(CFILES) $(PYFILES)
	python setup.py build_ext --include-dirs=$(TOPDIR)/src/include --library-dirs=$(TOPDIR)/src/libpcp/src
	python setup.py build
	touch s-setup

default: s-setup; @true

include $(BUILDRULES)

install_python: default
	python setup.py install --skip-build $(SETUP_PY_INSTALL_OPTIONS)

default_pcp:    default

install_pcp install: install_python
