#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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

LCFLAGS += -DPM_FAULT_INJECTION=1 -DPM_MULTI_THREAD_DEBUG=1

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CFILES = connect.c context.c desc.c err.c fetch.c freeresult.c \
	help.c instance.c p_desc.c p_error.c p_fetch.c p_instance.c \
	p_profile.c p_result.c p_text.c p_pmns.c p_creds.c p_attr.c \
	pdu.c pdubuf.c pmns.c profile.c store.c units.c util.c ipc.c \
	sortinst.c logmeta.c logportmap.c logutil.c tz.c interp.c \
	checksum.c rtime.c tv.c spec.c fetchlocal.c optfetch.c AF.c \
	stuffvalue.c endian.c config.c auxconnect.c auxserver.c discovery.c \
	p_lcontrol.c p_lrequest.c p_lstatus.c logconnect.c logcontrol.c \
	connectlocal.c derive.c derive_fetch.c events.c lock.c hash.c \
	fault.c access.c getopt.c probe.c
HFILES = derive.h internal.h avahi.h probe.h
YFILES = getdate.y
VERSION_SCRIPT = exports

LSRCFILES = check-statics $(VERSION_SCRIPT)

ifeq "$(ENABLE_SECURE)" "true"
LLDLIBS += $(LIB_FOR_SSL) $(LIB_FOR_NSS) $(LIB_FOR_NSPR) $(LIB_FOR_SASL)
LCFLAGS += $(NSSCFLAGS) $(NSPRCFLAGS) $(SASLCFLAGS)
CFILES += secureserver.c secureconnect.c
else
LSRCFILES += secureserver.c secureconnect.c
endif

ifeq "$(ENABLE_AVAHI)" "true"
LLDLIBS += $(LIB_FOR_AVAHI)
LCFLAGS += $(AVAHICFLAGS)
CFILES += avahi.c
else
LSRCFILES += avahi.c
endif

ifneq "$(TARGET_OS)" "mingw"
CFILES += accounts.c
LSRCFILES += win32.c
else
CFILES += win32.c
LSRCFILES += accounts.c
LLDLIBS	+= -lpsapi -lws2_32
endif

ifeq "$(TARGET_OS)" "solaris"
# enables standards compliant thread-safe interfaces (accounts.c)
LCFLAGS += -D_POSIX_PTHREAD_SEMANTICS
endif

ifeq "$(LIB_FOR_BASENAME)" "-lpcp"
# don't need to be linked to myself in this case!
LIB_FOR_BASENAME =
endif

LLDLIBS	+= $(LIB_FOR_MATH) $(LIB_FOR_PTHREADS) $(LIB_FOR_RT)
LLDLIBS	+= -lpcp_pmda

LCFLAGS += -DPCP_INTERNAL '-DEXEC_SUFFIX="$(EXECSUFFIX)"' \
	'-DDSO_SUFFIX="$(DSOSUFFIX)"'

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

LDIRT += $(SYMTARGET) $(YFILES:%.y=%.tab.?) getdate.h check.done

base default : $(HFILES) $(CFILES) $(VERSION_SCRIPT) $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

libpcp.so: $(SYMTARGET)
	ln -s $(SYMTARGET) libpcp.so

ifneq "$(SYMTARGET)" ""
$(SYMTARGET): 
	$(LN_S) -f $(LIBTARGET) $@
endif

include $(BUILDRULES)

*.o: internal.h
rtime.o: getdate.h
derive.o derive_fetch.o: derive.h
util.o:	$(TOPDIR)/src/include/pcp/pmdbg.h
fault.o:	$(TOPDIR)/src/include/pcp/fault.h

$(OBJECTS):	$(TOPDIR)/src/include/pcp/pmapi.h \
		$(TOPDIR)/src/include/pcp/impl.h \
		$(TOPDIR)/src/include/pcp/platform_defs.h

.NOTPARALLEL:
getdate.h getdate.tab.c: getdate.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@

ifeq "$(TARGET_OS)" "mingw"
kernel_pmda_dso	= windows
else
kernel_pmda_dso	= $(TARGET_OS)
endif

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

$(HFILES) $(CFILES) $(YFILES):
	ln -s ../../libpcp/src/$@ .

$(VERSION_SCRIPT): ../../libpcp/src/$(VERSION_SCRIPT) mk.exports
	./mk.exports

default_pcp : default

install_pcp : install

$(TOPDIR)/src/pmns/stdpmid:
	cd $(@D); $(MAKE) $(@F)

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
endif
