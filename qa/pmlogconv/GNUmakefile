#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
PATH	= $(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

# strip -I and -L options
#
TMP             := $(CFLAGS:-I%=)
CFLAGS          = $(TMP)
PCP_LIBS	=

ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"
# for cc add -I<run-time-include-dir> (need /.. at the end so
# #include <pcp/foo.h> works) when $(PCP_INC_DIR) may not be on
# the default cpp include search path.
CFLAGS		+= -I$(PCP_INC_DIR)/..
endif
ifneq "$(PCP_LIB_DIR)" "/usr/lib"
# for ld add -L<run-time-lib-dir> and include -rpath when
# $(PCP_LIB_DIR) may not be on the default ld search path.
#
ifeq "$(PCP_PLATFORM)" "darwin"
PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath $(PCP_LIB_DIR)
else
PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath=$(PCP_LIB_DIR)
endif
endif

CFILES = pmlogconv.c logio.c libpcp.c
LLDLIBS = -lpcp
TARGET = pmlogconv

LDIRT = $(TARGET)

default: $(TARGET)

$(TARGET): $(OBJECTS)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)

libpcp.o:	libpcp.c
logio.o:	logio.c
pmlogconv.o:	pmlogconv.c

libpcp.o logio.o pmlogconv.o:	$(TOPDIR)/src/include/pcp/libpcp.h
