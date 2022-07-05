#
# Copyright (c) 2013,2022 Red Hat.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmlogger$(EXECSUFFIX)

CFILES	= pmlogger.c fetch.c util.c error.c callback.c ports.c \
	  dopdu.c checks.c logue.c events.c pass0.c
HFILES	= logger.h
LFILES  = lex.l
YFILES	= gram.y

LCFLAGS += $(PIECFLAGS)
LLDFLAGS += $(PIELDFLAGS) $(LIB_FOR_BACKTRACE)

# Set $BACTRACE_SYMBOLS=something to build a binary with key function
# symbols defined and accessible from mybacktrace()
#
BACKTRACE_SYMBOLS=y
ifneq "$(BACKTRACE_SYMBOLS)" ""
LLDFLAGS += -rdynamic
LCFLAGS += -DBACKTRACE_SYMBOLS=1
LLDFLAGS += $(LIB_FOR_BACKTRACE)
endif

LLDLIBS	= $(PCP_ARCHIVELIB) $(PCPLIB) $(LIB_FOR_PTHREADS)
LDIRT	= *.log foo.* gram.h lex.c y.tab.? $(YFILES:%.y=%.tab.?) $(CMDTARGET)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -S $(PCP_BIN_DIR)/$(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

.NOTPARALLEL:
YFLAGS += -v
gram.tab.h gram.tab.c:	gram.y
	$(YACC) -d -b `basename $< .y` $<

lex.o gram.tab.o:	gram.tab.h

default_pcp:	default

install_pcp:	install

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h

foo:
	@echo $(BACKTRACE_SYMBOLS)
