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
	p_profile.c p_result.c p_text.c p_pmns.c p_creds.c pdu.c \
	pdubuf.c pmns.c profile.c store.c units.c util.c ipc.c \
        sortinst.c logmeta.c logportmap.c logutil.c tz.c interp.c \
	checksum.c rtime.c tv.c spec.c fetchlocal.c optfetch.c AF.c \
        stuffvalue.c endian.c config.c access.c auxconnect.c discovery.c \
	p_lcontrol.c p_lrequest.c p_lstatus.c logconnect.c logcontrol.c \
	connectlocal.c derive.c derive_fetch.c events.c lock.c hash.c \
	fault.c

HFILES = derive.h internal.h

ifneq "$(TARGET_OS)" "mingw"
LSRCFILES = win32.c
else
CFILES += win32.c
LLDLIBS	+= -lpsapi
endif

ifeq "$(LIB_FOR_BASENAME)" "-lpcp"
# don't need to be linked to myself in this case!
LIB_FOR_BASENAME =
endif

LLDLIBS	+= $(LIB_FOR_MATH) $(LIB_FOR_PTHREADS) -lpcp_pmda

LCFLAGS += -DLIBPCP_INTERNAL '-DEXEC_SUFFIX="$(EXECSUFFIX)"' \
	'-DDSO_SUFFIX="$(DSOSUFFIX)"'

ifeq "$(ENABLE_SECURE)" "true"
LLDLIBS += $(LIB_FOR_SSL) $(LIB_FOR_NSS) $(LIB_FOR_NSPR) $(LIB_FOR_SASL)
LCFLAGS += $(NSSCFLAGS) $(NSPRCFLAGS) $(SASLCFLAGS)
CFILES += secureserver.c secureconnect.c
else
LSRCFILES += secureserver.c secureconnect.c
endif

# same as libcp
DSOVERSION = 3
STATICLIBTARGET = libpcp_fault.a
LIBTARGET = libpcp_fault.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_fault.$(DSOSUFFIX)

ifeq "$(PACKAGE_DISTRIBUTION)" "debian"
SYMTARGET = libpcp_fault.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_fault.$(DSOVERSION).$(DSOSUFFIX)
SYMTARGET = libpcp_fault.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
STATICLIBTARGET = 
LIBTARGET = libpcp_fault.$(DSOSUFFIX)
SYMTARGET =
endif

LDIRT += $(SYMTARGET) libpcp.so

base default : $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) libpcp.so

libpcp.so: $(SYMTARGET)
	ln -s $(SYMTARGET) libpcp.so

ifneq "$(SYMTARGET)" ""
$(SYMTARGET): 
	$(LN_S) -f $(LIBTARGET) $@
endif

include $(BUILDRULES)

derive_fetch.o derive.o: derive.h
context.o context.o derive.o fetch.o lock.o: internal.h

util.o:	$(TOPDIR)/src/include/pcp/pmdbg.h

fetch.o: internal.h

fault.o:	$(TOPDIR)/src/include/pcp/fault.h

$(OBJECTS):	$(TOPDIR)/src/include/pcp/pmapi.h $(TOPDIR)/src/include/pcp/impl.h

ifeq "$(TARGET_OS)" "mingw"
kernel_pmda_dso	= windows
else
kernel_pmda_dso	= $(TARGET_OS)
endif

install : default
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
ifneq "$(SYMTARGET)" ""
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq "$(STATICLIBTARGET)" ""
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif
	$(INSTALL) -m 644 $(TOPDIR)/src/include/pcp/fault.h $(PCP_INC_DIR)/fault.h

$(HFILES) $(CFILES):
	ln -s ../../libpcp/src/$@ .

default_pcp : default

install_pcp : install

$(TOPDIR)/src/pmns/stdpmid:
	cd $(@D); $(MAKE) $(@F)
