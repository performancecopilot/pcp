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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IMAGES = blinken_error.png blinken_off.png blinken_on.png
JSFILES = blinkenlights.js
CSSFILES = blinkenlights.css
HTMLFILES = index.html

LSRCFILES = $(IMAGES) $(JSFILES) $(CSSFILES) $(HTMLFILES)

default:

include $(BUILDRULES)

install:

default_pcp:	default

install_pcp:	install
