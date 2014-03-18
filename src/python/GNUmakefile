#
# Copyright (c) 2012-2013 Red Hat.
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

SUBDIRS = pcp
CFILES = pmapi.c pmda.c pmgui.c pmi.c mmv.c
LSRCFILES = setup.py
LDIRDIRT = build dist
LDIRT = build_python
ifneq ($(CFLAGS_ABI),)
ENV = CFLAGS=$(CFLAGS_ABI)
else
ENV =
endif

default default_pcp: build_python

ifneq "$(PYTHON)" ""
build_python: setup.py $(CFILES)
	$(ENV) $(PYTHON) setup.py build_ext $(SETUP_PY_BUILD_OPTIONS)
	$(ENV) $(PYTHON) setup.py build
	touch build_python

install_python: default
	$(PYTHON) setup.py install $(SETUP_PY_INSTALL_OPTIONS)
	$(PYTHON_INSTALL)
else
build_python: 
install_python: 
endif

include $(BUILDRULES)

install_pcp install: install_python
