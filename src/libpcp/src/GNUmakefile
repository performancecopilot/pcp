#
# Copyright (c) 2012-2022 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
include ./GNUlibrarydefs
-include ./GNUlocaldefs

# NOTE:
# 	if you add source files or header files here, you need to make
# 	the SAME change in ../../libpcp_static/src/GNUmakefile AND
# 	../../libpcp_fault/src/GNUmakefile

JSONSL_HFILES = $(addprefix deps/jsonsl/, jsonsl.h)
JSONSL_CFILES = $(addprefix deps/jsonsl/, jsonsl.c)
JSONSL_XFILES = $(JSONSL_HFILES) $(JSONSL_CFILES)

CFILES = connect.c context.c desc.c err.c fetch.c fetchgroup.c result.c \
	help.c instance.c labels.c \
	p_attr.c p_desc.c p_error.c p_fetch.c p_idlist.c p_instance.c \
	p_profile.c p_result.c p_text.c p_pmns.c p_creds.c p_label.c \
	pdu.c pdubuf.c pmns.c profile.c store.c units.c util.c ipc.c \
	sortinst.c logmeta.c logportmap.c logutil.c tz.c interp.c \
	rtime.c tv.c spec.c fetchlocal.c optfetch.c AF.c \
	stuffvalue.c endian.c config.c auxconnect.c auxserver.c discovery.c \
	p_lcontrol.c p_lrequest.c p_lstatus.c logconnect.c logcontrol.c \
	connectlocal.c derive_fetch.c events.c lock.c hash.c \
	fault.c access.c getopt.c getopt2.c getopt3.c \
	io.c io_stdio.c exec.c sha256.c strings.c \
	shellprobe.c subnetprobe.c deprecated.c \
	e_loglabel.c e_index.c e_indom.c e_labels.c \
	$(JSONSL_CFILES)
HFILES = derive.h internal.h compiler.h pmdbg.h sha256.h sort_r.h \
	avahi.h subnetprobe.h shellprobe.h \
	$(JSONSL_HFILES)
XFILES = sha256.c sha256.h sort_r.h
YFILES = getdate.y derive_parser.y
GENERATED_HFILES = pmdbg.h

LIBCONFIG = libpcp.pc
VERSION_SCRIPT = exports

LCFLAGS = '-DEXEC_SUFFIX="$(EXECSUFFIX)"' '-DDSO_SUFFIX="$(DSOSUFFIX)"'
LCFLAGS += $(LIBPCP_CFLAGS) -DPCP_INTERNAL -Ideps
LLDLIBS += $(LIBPCP_LDLIBS) $(LIB_FOR_BACKTRACE)

ifeq "$(ENABLE_SECURE)" "true"
CFILES += secureserver.c secureconnect.c
endif

ifeq "$(ENABLE_AVAHI)" "true"
CFILES += avahi.c
endif

ifeq "$(ENABLE_LZMA)" "true"
CFILES += io_xz.c
endif

ifneq "$(TARGET_OS)" "mingw"
CFILES += accounts.c
else
CFILES += win32.c
endif

DSOVERSION = 3
STATICLIBTARGET = libpcp.a
LIBTARGET = libpcp.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp.$(DSOSUFFIX)

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp.$(DSOVERSION).$(DSOSUFFIX)
SYMTARGET = libpcp.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
STATICLIBTARGET =
LIBTARGET = libpcp.$(DSOSUFFIX)
SYMTARGET =
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LDIRT += $(SYMTARGET) $(YFILES:%.y=%.tab.?) $(XFILES) $(LIBCONFIG) \
	getdate.h derive_parser.y check.done $(GENERATED_HFILES) exports \
	$(JSONSL_XFILES)

base default : $(XFILES) $(JSONSL_XFILES) \
	$(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) \
	$(LIBCONFIG) check.done

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

include $(BUILDRULES)

*.o: internal.h compiler.h derive.h
sha256.o: sha256.c sha256.h
rtime.o: getdate.h
derive.o derive_fetch.o: derive.h derive_parser.tab.h
util.o:	pmdbg.h
labels.o: sha256.h sort_r.h

$(OBJECTS):	$(TOPDIR)/src/include/pcp/pmapi.h \
		$(TOPDIR)/src/include/pcp/platform_defs.h \
		$(GENERATED_HFILES)

.NOTPARALLEL:
getdate.h getdate.tab.c: getdate.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@

.NOTPARALLEL:
derive_parser.tab.h derive_parser.tab.c: derive_parser.y derive.h
	$(YACC) -d -b `basename $< .y` $<

derive_parser.y: derive_parser.y.in fix_derive_parser_y
	./fix_derive_parser_y

ifeq "$(TARGET_OS)" "mingw"
kernel_pmda_dso	= windows
else
kernel_pmda_dso	= $(TARGET_OS)
endif

install : default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	$(INSTALL) -m 644 $(LIBCONFIG) $(PCP_LIB_DIR)/pkgconfig/$(LIBCONFIG)
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

$(XFILES):
	$(LN_S) $(TOPDIR)/src/external/$@ .

$(JSONSL_XFILES):
	mkdir -p deps/jsonsl
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/mnunberg/$(@:deps/%=%) $@

default_pcp : default

install_pcp : install

$(TOPDIR)/src/pmns/stdpmid:
	cd $(@D); $(MAKE) $(@F)

pmdbg.h : ../../include/pcp/pmapi.h mk.pmdbg
	./mk.pmdbg

exports:	exports.in mk.exports
	./mk.exports

# The library is thread-safe ... check-statics will force a build failure
# if there has been any change to the static variables and their disposition
# ... refer to check-statics to add exceptions and annotations for new
# cases
#
check.done:	$(OBJECTS)
	./check-statics
	touch check.done

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
$(LIBCONFIG): $(LIBCONFIG).in
	@sed -e's|@PREFIX@|$(PCP_PREFIX)|g' \
	     -e's|@LIBDIR@|$(PCP_LIB_DIR)|g' \
	     -e's|@LDLIBS@|$(LIBPCP_LDLIBS)|g' \
	     -e's|@INCDIR@|$(PCP_INC_DIR)|g' \
	     -e's|@VERSION@|$(PCP_VERSION)|g' <$? >$@
endif

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h

check::	$(CFILES) $(HFILES)
	$(CLINT) $^
