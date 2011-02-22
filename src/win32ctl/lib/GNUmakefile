#!gmake
#
# Copyright (c) 2010-2011 Aconex.  All Rights Reserved.
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

ifeq "$(TARGET_OS)" "mingw"
STATICLIBS = libpcp_pdh.a libpcp_tdh.a
endif
STATICDEFS = libpcp_pdh.def libpcp_tdh.def
MKWIN32LIB = sh -c "$(DLLTOOL) -d $< -k -l $@"

LSRCFILES = $(STATICDEFS)
LDIRT = $(STATICLIBS)

default: $(STATICLIBS)

include $(BUILDRULES)

libpcp_pdh.a:	libpcp_pdh.def
	$(MKWIN32LIB)

libpcp_tdh.a:	libpcp_tdh.def
	$(MKWIN32LIB)

default_pcp:	default

install:	default

install_pcp:	install
