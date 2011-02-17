#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include /etc/pcp.conf
endif
PATH	= $(shell . /etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

CFILES = pmlogconv.c logio.c libpcp.c
LLDLIBS = -lpcp
TARGET = pmlogconv

LDIRT = $(TARGET)

default : $(TARGET)

$(TARGET) : $(OBJECTS)

include $(PCP_INC_DIR)/buildrules

install : default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)

pmlogconv.o:	pmlogconv.c
logio.o:	logio.c
libpcp.o:	libpcp.c
