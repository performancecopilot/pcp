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

# for cc replace all the -I<path> options with -I<run-time-include-dir>
# (need /.. at the end so #include <pcp/foo.h> works)
#
TMP             := $(CFLAGS:-I%=)
CFLAGS          = $(TMP) -I$(PCP_INC_DIR)/..

# for ld replace -L<build-time-dirs> with -L<run-time-lib-dir>
# and include -rpath in case $(PCP_LIB_DIR) is not on the
# default ld search path, e.g. for NetBSD
#
PCP_LIBS         = -L$(PCP_LIB_DIR) -rpath=$(PCP_LIB_DIR)

CFILES = pmlogconv.c logio.c libpcp.c
LLDLIBS = -lpcp
TARGET = pmlogconv

LDIRT = $(TARGET)

default: $(TARGET)

$(TARGET): $(OBJECTS)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)

pmlogconv.o:	pmlogconv.c
logio.o:	logio.c
libpcp.o:	libpcp.c
