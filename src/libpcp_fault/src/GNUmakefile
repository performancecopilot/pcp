#
# Copyright (c) 2012-2021 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
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
include ../../libpcp/src/GNUlibrarydefs
-include ./GNUlocaldefs

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
	fault.c access.c getopt.c getopt_v2.c \
	io.c io_stdio.c exec.c sha256.c strings.c \
	shellprobe.c subnetprobe.c deprecated.c equivindom.c \
	e_loglabel.c e_index.c e_indom.c e_labels.c throttle.c \
	$(JSONSL_CFILES)
HFILES = derive.h internal.h compiler.h pmdbg.h sha256.h sort_r.h \
	avahi.h subnetprobe.h shellprobe.h \
	$(JSONSL_HFILES)
XFILES = sha256.c sha256.h sort_r.h
YFILES = getdate.y derive_parser.y
GENERATED_HFILES = pmdbg.h

VERSION_SCRIPT = exports

# files that will be linked to here from libpcp directory
CEXCLUDE = $(JSONSL_CFILES) $(XFILES)
HEXCLUDE = $(JSONSL_HFILES) $(XFILES) pmdbg.h
LN_FILES = getdate.y mk.pmdbg fix_derive_parser_y derive_parser.y.in
LN_FILES += $(filter-out $(CEXCLUDE), $(CFILES))
LN_FILES += $(filter-out $(HEXCLUDE), $(HFILES))

CFLAGS_OPT = -DPM_FAULT_INJECTION=1 -DPM_MULTI_THREAD_DEBUG=1 -g -O0

LCFLAGS = '-DEXEC_SUFFIX="$(EXECSUFFIX)"' '-DDSO_SUFFIX="$(DSOSUFFIX)"'
LCFLAGS += $(LIBPCP_CFLAGS) -DPCP_INTERNAL -Ideps
LLDLIBS	+= $(LIBPCP_LDLIBS) $(PCP_PMDALIB)

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

ifeq "$(TARGET_OS)" "solaris"
# enables standards compliant thread-safe interfaces (accounts.c)
LCFLAGS += -D_POSIX_PTHREAD_SEMANTICS
endif

ifeq "$(LIB_FOR_BASENAME)" "-lpcp"
# don't need to be linked to myself in this case!
LIB_FOR_BASENAME =
endif

DSOVERSION = 3
STATICLIBTARGET = libpcp_fault.a
LIBTARGET = libpcp_fault.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_fault.$(DSOSUFFIX)

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_fault.$(DSOVERSION).$(DSOSUFFIX)
SYMTARGET = libpcp_fault.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
STATICLIBTARGET = 
LIBTARGET = libpcp_fault.$(DSOSUFFIX)
SYMTARGET =
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET = 
SYMTARGET =
endif

LDIRT += $(SYMTARGET) $(YFILES:%.y=%.tab.?) *.c check.done \
	$(LN_FILES) $(XFILES) $(JSONSL_XFILES) exports \
	getdate.h derive_parser.y pmdbg.h

base default : $(LN_FILES) $(XFILES) $(JSONSL_XFILES) \
	$(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) \
	$(VERSION_SCRIPT) $(INFILES)

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
fault.o:	$(TOPDIR)/src/include/pcp/fault.h

$(OBJECTS):	$(TOPDIR)/src/include/pcp/pmapi.h \
		$(TOPDIR)/src/include/pcp/platform_defs.h

.NOTPARALLEL:
getdate.h getdate.tab.c: getdate.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@

.NOTPARALLEL:
derive_parser.tab.h derive_parser.tab.c: derive_parser.y derive.h
	$(YACC) -d -b `basename $< .y` $<

derive_parser.y: derive_parser.y.in fix_derive_parser_y
	./fix_derive_parser_y

install : default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif
	$(INSTALL) -m 644 $(TOPDIR)/src/include/pcp/fault.h $(PCP_INC_DIR)/fault.h

pmdbg.h : ../../include/pcp/pmapi.h mk.pmdbg
	./mk.pmdbg

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
endif

$(VERSION_SCRIPT): ../../libpcp/src/$(VERSION_SCRIPT) mk.exports
	./mk.exports

../../libpcp/src/$(VERSION_SCRIPT):
	( cd ../../libpcp/src; ./mk.exports )

$(LN_FILES):
	@rm -f $@
	$(LN_S) $(TOPDIR)/src/libpcp/src/$@ .

$(XFILES):
	@rm -f $@
	$(LN_S) $(TOPDIR)/src/external/$@ .

$(JSONSL_XFILES):
	mkdir -p deps/jsonsl
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/mnunberg/$(@:deps/%=%) $@

default_pcp : default

install_pcp : install

$(TOPDIR)/src/pmns/stdpmid:
	cd $(@D); $(MAKE) $(@F)
