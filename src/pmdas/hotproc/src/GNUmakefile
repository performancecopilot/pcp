#!gmake
#
# Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

# This is just to get the source exported to the community.
# 'Makefile' in particular is included only for clues it might offer,
# and should almost certainly be removed from SRCFILES once
# the pmda is working.
LSRCFILES =  Makefile config.c config.h ctltab.c error.c gram.y \
gram_node.c gram_node.h hotproc.c hotproc.h lex.l pcpu.c pcpu.h \
pglobal.c pglobal.h ppred_values.c ppred_values.h

default default_pcp: 
install install_pcp:
include $(BUILDRULES)

