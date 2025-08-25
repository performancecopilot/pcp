#
# Copyright (c) 2012-2016,2025 Red Hat.
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
LDIRDIRT = build dist pcp.egginfo pcp.egg-info
LDIRT = build_python3 MANIFEST

ifneq ($(CFLAGS_ABI),)
ENV = CFLAGS=$(CFLAGS_ABI)
else
ENV = nothing=''
endif
ifeq ($(PCP_PLATFORM),mingw)
ENV += CC="$(CC)"
ENV += LD="$(LD)"
ENV += LDSHARED="$(CC) -shared -fstack-protector"
ENV += PYTHONCPREFIX="/usr/x86_64-w64-mingw32/sys-root/mingw/"
# investigate pkgconfig for this (version-specific currently & missing libs?)
# /usr/x86_64-w64-mingw32/sys-root/mingw/lib/pkgconfig/python3-embed.pc
EXTRA_PY_INCLUDES = :/usr/x86_64-w64-mingw32/sys-root/mingw/include:/usr/x86_64-w64-mingw32/sys-root/mingw/include/python3.11
endif
ifeq "$(HAVE_PYTHON_SETUPTOOLS)" "true"
SETUP_PY = setup.py
else
SETUP_PY = distutils-setup.py
endif

ifeq ($(PCP_PLATFORM),darwin)
default default_pcp: pre_build build_python3 fix_install_names
else
default default_pcp: pre_build build_python3
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

install_pcp install: install_python3

test:
	pmpython -m unittest discover -s test -p '*_test.py'

build_python3:	$(TOPDIR)/src/include/pcp/libpcp.h

# deal with any configurable Python source, e.g. pmapi.py.in
#
pre_build:
	cd pcp; $(MAKE) default

fix_install_names:
	@for so in build/lib.macosx-*/c*.so; do \
		if [ -f "$$so" ]; then \
			echo "Fixing library references in $$so"; \
			for lib in $$(otool -L "$$so" | grep -E 'libpcp.*\.dylib' | awk '{print $$1}'); do \
				if echo "$$lib" | grep -q -v '@rpath'; then \
					newlib=$$(echo "$$lib" | sed 's|libpcp[^/]*\.dylib|@rpath/libpcp.dylib|'); \
					newlib=$$(echo "$$newlib" | sed 's|libpcp_pmda[^/]*\.dylib|@rpath/libpcp_pmda.dylib|'); \
					newlib=$$(echo "$$newlib" | sed 's|libpcp_gui[^/]*\.dylib|@rpath/libpcp_gui.dylib|'); \
					newlib=$$(echo "$$newlib" | sed 's|libpcp_import[^/]*\.dylib|@rpath/libpcp_import.dylib|'); \
					newlib=$$(echo "$$newlib" | sed 's|libpcp_mmv[^/]*\.dylib|@rpath/libpcp_mmv.dylib|'); \
					echo "Changing $$lib to $$newlib"; \
					install_name_tool -change "$$lib" "$$newlib" "$$so"; \
				fi; \
			done; \
		fi; \
	done

foo:
	echo $(CFLAGS)
