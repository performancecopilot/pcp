#
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CSSFILES = $(shell echo *.css)
HTMLFILES = $(shell echo *.html)
LSRCFILES = $(HTMLFILES) $(CSSFILES)

SUBDIRS = images cpuperf diskmodel diskperf importdata pmie pmview

default :: default_pcp 

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PCP_BOOKS_DIR)/html
	$(INSTALL) -m 644 $(HTMLFILES) $(CSSFILES) $(PCP_BOOKS_DIR)/html
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
