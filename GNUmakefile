#
# Copyright (c) 2013 Red Hat.  All Rights Reserved.
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

JSFILES = jquery-1.7.2.js jquery-ui-1.10.2.js jquery-ui-themes-1.10.2.tar.gz
SUBDIRS = blinkenlights jsbrowser
LSRCFILES = $(JSFILES) favicon.ico
LDIRDIRT = jquery-ui-themes-1.10.2
LDIRT = .unpacked

default:	$(SUBDIRS) jquery-ui-themes
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

jquery-ui-themes:	.unpacked
.unpacked:	jquery-ui-themes-1.10.2.tar.gz
	$(ZIP) -d < $^ | $(TAR) -xf -
	touch .unpacked

install:	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)

default_pcp:	default

install_pcp:	install
