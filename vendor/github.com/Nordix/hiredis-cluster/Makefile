# Hiredis-cluster Makefile, based on the Makefile in hiredis.
#
# Copyright (C) 2021 Bjorn Svensson <bjorn.a.svensson@est.tech>
# Copyright (C) 2010-2011 Salvatore Sanfilippo <antirez at gmail dot com>
# Copyright (C) 2010-2011 Pieter Noordhuis <pcnoordhuis at gmail dot com>
# This file is released under the BSD license, see the COPYING file

OBJ=adlist.o command.o crc16.o dict.o hiarray.o hircluster.o hiutil.o
EXAMPLES=hiredis-cluster-example hiredis-cluster-example-tls
LIBNAME=libhiredis_cluster
PKGCONFNAME=hiredis_cluster.pc

HIREDIS_CLUSTER_MAJOR=$(shell grep HIREDIS_CLUSTER_MAJOR hircluster.h | awk '{print $$3}')
HIREDIS_CLUSTER_MINOR=$(shell grep HIREDIS_CLUSTER_MINOR hircluster.h | awk '{print $$3}')
HIREDIS_CLUSTER_PATCH=$(shell grep HIREDIS_CLUSTER_PATCH hircluster.h | awk '{print $$3}')
HIREDIS_CLUSTER_SONAME=$(shell grep HIREDIS_CLUSTER_SONAME hircluster.h | awk '{print $$3}')

# Installation related variables and target
PREFIX?=/usr/local
INCLUDE_PATH?=include/hiredis_cluster
LIBRARY_PATH?=lib
PKGCONF_PATH?=pkgconfig
INSTALL_INCLUDE_PATH= $(DESTDIR)$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH= $(DESTDIR)$(PREFIX)/$(LIBRARY_PATH)
INSTALL_PKGCONF_PATH= $(INSTALL_LIBRARY_PATH)/$(PKGCONF_PATH)

# Fallback to gcc when $CC is not in $PATH.
CC:=$(shell sh -c 'type $${CC%% *} >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
OPTIMIZATION?=-O3
WARNINGS=-Wall -W -Wstrict-prototypes -Wwrite-strings
DEBUG_FLAGS?= -g -ggdb
REAL_CFLAGS=$(OPTIMIZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG_FLAGS)
REAL_LDFLAGS=$(LDFLAGS)

DYLIBSUFFIX=so
STLIBSUFFIX=a
DYLIB_MINOR_NAME=$(LIBNAME).$(DYLIBSUFFIX).$(HIREDIS_CLUSTER_SONAME)
DYLIB_MAJOR_NAME=$(LIBNAME).$(DYLIBSUFFIX).$(HIREDIS_CLUSTER_MAJOR)
DYLIBNAME=$(LIBNAME).$(DYLIBSUFFIX)

DYLIB_MAKE_CMD=$(CC) -shared -Wl,-soname,$(DYLIB_MINOR_NAME)
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=$(AR) rcs

USE_SSL?=0

# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(USE_SSL),1)
ifeq ($(uname_S),Linux)
  REAL_CFLAGS+=-DSSL_SUPPORT
  REAL_LDFLAGS+=-lssl -lcrypto
endif
endif

all: $(DYLIBNAME) $(STLIBNAME) $(PKGCONFNAME)

# Deps (use make dep to generate this)
adlist.o: adlist.c adlist.h hiutil.h
command.o: command.c command.h adlist.h hiarray.h hiutil.h
crc16.o: crc16.c hiutil.h
dict.o: dict.c dict.h
hiarray.o: hiarray.c hiarray.h hiutil.h
hircluster.o: hircluster.c adlist.h command.h dict.h hiarray.h \
 hircluster.h hiutil.h win32.h
hiutil.o: hiutil.c hiutil.h win32.h

$(DYLIBNAME): $(OBJ)
	$(DYLIB_MAKE_CMD) -o $(DYLIBNAME) $(OBJ) $(REAL_LDFLAGS)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(STLIBNAME) $(OBJ)

dynamic: $(DYLIBNAME)
static: $(STLIBNAME)

# Binaries:
hiredis-cluster-example: examples/src/example.c
	$(CC) -o examples/$@ $(REAL_CFLAGS) $< $(REAL_LDFLAGS)
hiredis-cluster-example-tls: examples/src/example_tls.c
	$(CC) -o examples/$@ $(REAL_CFLAGS) $< $(REAL_LDFLAGS)

examples: $(EXAMPLES)

.c.o:
	$(CC) -std=c99 -pedantic -c $(REAL_CFLAGS) $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) $(SSL_DYLIBNAME) $(PKGCONFNAME) examples/hiredis-cluster-example* *.o *.gcda *.gcno *.gcov

dep:
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM *.c

INSTALL?= cp -pPR

$(PKGCONFNAME): hircluster.h
	@echo "Generating $@ for pkgconfig..."
	@echo prefix=$(PREFIX) > $@
	@echo exec_prefix=\$${prefix} >> $@
	@echo libdir=$(PREFIX)/$(LIBRARY_PATH) >> $@
	@echo includedir=$(PREFIX)/$(INCLUDE_PATH) >> $@
	@echo >> $@
	@echo Name: hiredis-cluster >> $@
	@echo Description: Minimalistic C client library for Redis Cluster. >> $@
	@echo Version: $(HIREDIS_CLUSTER_MAJOR).$(HIREDIS_CLUSTER_MINOR).$(HIREDIS_CLUSTER_PATCH) >> $@
	@echo Libs: -L\$${libdir} -lhiredis_cluster >> $@
	@echo Cflags: -I\$${includedir} -D_FILE_OFFSET_BITS=64 >> $@

install: $(DYLIBNAME) $(STLIBNAME) $(PKGCONFNAME)
	mkdir -p $(INSTALL_INCLUDE_PATH) $(INSTALL_INCLUDE_PATH)/adapters $(INSTALL_LIBRARY_PATH)
	$(INSTALL) adlist.h dict.h hiarray.h hircluster.h hiutil.h win32.h $(INSTALL_INCLUDE_PATH)
	$(INSTALL) adapters/*.h $(INSTALL_INCLUDE_PATH)/adapters
	$(INSTALL) $(DYLIBNAME) $(INSTALL_LIBRARY_PATH)/$(DYLIB_MINOR_NAME)
	cd $(INSTALL_LIBRARY_PATH) && ln -sf $(DYLIB_MINOR_NAME) $(DYLIBNAME)
	$(INSTALL) $(STLIBNAME) $(INSTALL_LIBRARY_PATH)
	mkdir -p $(INSTALL_PKGCONF_PATH)
	$(INSTALL) $(PKGCONFNAME) $(INSTALL_PKGCONF_PATH)

32bit:
	@echo ""
	@echo "WARNING: if this fails under Linux you probably need to install libc6-dev-i386"
	@echo ""
	$(MAKE) CFLAGS="-m32" LDFLAGS="-m32"

32bit-vars:
	$(eval CFLAGS=-m32)
	$(eval LDFLAGS=-m32)

gprof:
	$(MAKE) CFLAGS="-pg" LDFLAGS="-pg"

gcov:
	$(MAKE) CFLAGS="-fprofile-arcs -ftest-coverage" LDFLAGS="-fprofile-arcs"

noopt:
	$(MAKE) OPTIMIZATION=""

.PHONY: all clean dep install 32bit 32bit-vars gprof gcov noopt
