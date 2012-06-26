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

# remove -Lpath and -Ipath options from builddefs CFLAGS value
#
PCP_LIBS        =
TMP             := $(CFLAGS:-I%=)
ifdef PCP_DIR
# put -Ipath and -Lpath back but use paths for run-time environment
#
CFLAGS          = $(TMP) -I$(PCP_INC_DIR)/..
LDFLAGS         = -L$(PCP_LIB_DIR)
else
CFLAGS          = $(TMP)
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

pmlogconv.o:	pmlogconv.c
logio.o:	logio.c
libpcp.o:	libpcp.c
