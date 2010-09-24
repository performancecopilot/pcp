#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

STATICLIBTARGET = libpcp_http.a
LCFLAGS = -DVERSION=\"1.1.0\"
CFILES = http_error_codes.c http_fetcher.c
HFILES = http_error_codes.h http_fetcher.h
LSRCFILES = README

base default : $(STATICLIBTARGET)

include $(BUILDRULES)

install : default

default_pcp : default

install_pcp : install
