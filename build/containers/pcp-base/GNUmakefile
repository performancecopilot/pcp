#
# Copyright (c) 2015 Red Hat.
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
-include ./GNUlocaldefs

CONTAINER = pcp-base
LDIRT = $(CONTAINER).tgz RPMS

default default_pcp : RPMS $(CONTAINER).tgz

RPMS:
	# symlink doesnt work with docker build
	mkdir RPMS; cp -a ../RPMS/* RPMS

install install_pcp:

# rule to build $(CONTAINER).tgz from Dockerfile
include ../GNUlocalrules

include $(BUILDRULES)
