#
# Copyright (c) 2000,2004,2012 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
prefix = @prefix@

PYFILES = pcpi.py pcp.py pm-collectl.py pmapi.c


s-setup : $(PYFILES)
	python setup.py build
	touch s-setup

default: s-setup; @true

include $(BUILDRULES)

# add  --root %(prefix) when prefix is available
install: 
	python setup.py install --skip-build

default_pcp:    default

install_pcp:    install
