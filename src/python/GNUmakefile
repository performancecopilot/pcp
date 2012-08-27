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

SETUP_PY_BUILD_OPTIONS = --include-dirs=$(TOPDIR)/src/include
SETUP_PY_BUILD_OPTIONS += --library-dirs=$(TOPDIR)/src/libpcp/src

SETUP_PY_INSTALL_OPTIONS = --skip-build
SETUP_PY_INSTALL_OPTIONS += --prefix=$(PYTHON_PREFIX)
SETUP_PY_INSTALL_OPTIONS += --root=$${DIST_ROOT-/}
SETUP_PY_INSTALL_OPTIONS += --record=$(TOPDIR)/python-pcp.list

CFILES = pmapi.c
PYFILES = pcpi.py pcp.py setup.py
LDIRDIRT = build dist
LDIRT = build_python

default default_pcp: build_python

ifneq "$(PYTHON)" ""
build_python: $(CFILES) $(PYFILES)
	$(PYTHON) setup.py build_ext $(SETUP_PY_BUILD_OPTIONS)
	$(PYTHON) setup.py build
	touch build_python

install_python: default
	$(PYTHON) setup.py install $(SETUP_PY_INSTALL_OPTIONS)
	# RPM byte-compiles and installs results in DIST_ROOT:
	$(AWK) '{print} /.pyc$$/ {sub(/.pyc$$/,".pyo"); print}' \
		< $(TOPDIR)/python-pcp.list > $(TOPDIR)/python-pcp.list.rpm
else
build_python: 
install_python: 
endif

include $(BUILDRULES)

install_pcp install: install_python
