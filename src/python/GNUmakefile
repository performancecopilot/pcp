#
# Copyright (c) 2012-2014 Red Hat.
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
LDIRDIRT = build dist
LDIRT = build_python build_python3
ifneq ($(CFLAGS_ABI),)
ENV = CFLAGS=$(CFLAGS_ABI)
else
ENV =
endif
ifeq ($(PCP_PLATFORM),darwin)
# Universal binaries for PPC, thanks
ENV += ARCHFLAGS='-arch i386 -arch x86_64'
endif

default default_pcp: build_python build_python3

ifeq "$(ENABLE_PYTHON2)" "true"
PY_BUILD_OPTS = $(SETUP_PY_BUILD_OPTIONS)
PY_INSTALL_OPTS = $(SETUP_PY_INSTALL_OPTIONS) --record=$(TOPDIR)/python-pcp.list
build_python: setup.py $(CFILES)
	$(ENV) $(PYTHON) setup.py build_ext $(PY_BUILD_OPTS)
	$(ENV) $(PYTHON) setup.py build
	touch build_python

install_python: default
	$(PYTHON) setup.py install $(PY_INSTALL_OPTS)
	$(PYTHON_INSTALL)
else
build_python: 
install_python: 
endif

ifeq "$(ENABLE_PYTHON3)" "true"
PY3_BUILD_OPTS = $(SETUP_PY_BUILD_OPTIONS)
PY3_INSTALL_OPTS = $(SETUP_PY_INSTALL_OPTIONS) --record=$(TOPDIR)/python3-pcp.list
build_python3: setup.py $(CFILES)
	$(ENV) $(PYTHON3) setup.py build_ext $(PY3_BUILD_OPTS)
	$(ENV) $(PYTHON3) setup.py build
	touch build_python3

install_python3: default
	$(PYTHON3) setup.py install $(PY3_INSTALL_OPTS)
	$(PYTHON3_INSTALL)
else
build_python3:
install_python3: 
endif

include $(BUILDRULES)

install_pcp install: install_python install_python3
