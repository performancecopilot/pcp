#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

TEMPLATES = pcp.info.in pcp.SlackBuild.in
CONFFILES = pcp.info pcp.SlackBuild
LDIRT += $(CONFFILES)
LSRCFILES = $(TEMPLATES) $(CONFIGURE) README slack-desc

default install : $(CONFFILES)

pack_pcp : 

pcp.info: pcp.info.in
	sed -e's|@package_name@|$(PACKAGE_NAME)|g' \
	    -e's|@package_version@|$(PACKAGE_VERSION)|g' < $? > $@

pcp.SlackBuild: pcp.SlackBuild.in
	sed -e's|@package_name@|$(PACKAGE_NAME)|g' \
	    -e's|@package_build@|$(PACKAGE_BUILD)|g' \
	    -e's|@package_version@|$(PACKAGE_VERSION)|g' < $? > $@

include $(BUILDRULES)

