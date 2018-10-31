#
# Copyright (c) 2012-2016 Red Hat.
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

TOPDIR = $(shell pwd)/../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = pcp
CFILES = pmapi.c pmda.c pmgui.c pmi.c mmv.c
LDIRDIRT = build dist pcp.egginfo
LDIRT = build_python2 build_python3 MANIFEST

ifneq ($(CFLAGS_ABI),)
ENV = CFLAGS=$(CFLAGS_ABI)
else
ENV = nothing=''
endif
ifeq ($(PCP_PLATFORM),darwin)
# Universal binaries for Mac OS X
ENV += ARCHFLAGS='-arch i386 -arch x86_64'
endif
ifeq ($(PCP_PLATFORM),mingw)
ENV += CC="$(CC)"
ENV += LD="$(LD)"
ENV += LDSHARED="$(CC) -shared -fstack-protector"
ENV += PYTHONCPREFIX="/usr/x86_64-w64-mingw32/sys-root/mingw/"
EXTRA_PY_INCLUDES = :/usr/x86_64-w64-mingw32/sys-root/mingw/include:/usr/x86_64-w64-mingw32/sys-root/mingw/include/python2.7
endif
ifeq "$(HAVE_PYTHON_SETUPTOOLS)" "true"
SETUP_PY = setup.py
else
SETUP_PY = distutils-setup.py
endif

default default_pcp: build_python2 build_python3

ifeq "$(ENABLE_PYTHON2)" "true"
PY_BUILD_OPTS = $(SETUP_PY_BUILD_OPTIONS)
PY_INSTALL_OPTS = $(SETUP_PY_INSTALL_OPTIONS) --record=$(TOPDIR)/python-pcp.list
build_python2: $(SETUP_PY) $(CFILES)
	export $(ENV); $(PYTHON) $(SETUP_PY) build_ext $(PY_BUILD_OPTS)
	export $(ENV); $(PYTHON) $(SETUP_PY) build
	touch build_python2

install_python2: build_python2
	export $(ENV); $(PYTHON) $(SETUP_PY) install $(PY_INSTALL_OPTS)
	export $(ENV); $(PYTHON_INSTALL)
else
build_python2:
install_python2:
endif

ifeq "$(ENABLE_PYTHON3)" "true"
PY3_BUILD_OPTS = $(SETUP_PY_BUILD_OPTIONS)
PY3_INSTALL_OPTS = $(SETUP_PY_INSTALL_OPTIONS) --record=$(TOPDIR)/python3-pcp.list
build_python3: $(SETUP_PY) $(CFILES)
	export $(ENV); $(PYTHON3) $(SETUP_PY) build_ext $(PY3_BUILD_OPTS)
	export $(ENV); $(PYTHON3) $(SETUP_PY) build
	touch build_python3

install_python3: build_python3
	export $(ENV); $(PYTHON3) $(SETUP_PY) install $(PY3_INSTALL_OPTS)
	export $(ENV); $(PYTHON3_INSTALL)
else
build_python3:
install_python3:
endif

include $(BUILDRULES)

install_pcp install: install_python2 install_python3

check::	$(CFILES)
	$(CLINT) $^

test:
	pmpython -m unittest discover -s test -p '*_test.py'

build_python2 build_python3:	$(TOPDIR)/src/include/pcp/libpcp.h
