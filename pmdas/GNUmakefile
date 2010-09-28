#!gmake
#
# Copyright (c) 2010 Aconex.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include /etc/pcp.conf
endif
PATH	= $(shell . /etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

SUBDIRS = broken

default: 

install:

