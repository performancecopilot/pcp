#!gmake
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES	= pmlogextract.c logio.c error.c metriclist.c
HFILES	= logger.h
LFILES  = lex.l
YFILES	= gram.y

CMDTARGET = pmlogextract$(EXECSUFFIX)

LLDLIBS	= -lpcp -lm $(LIB_FOR_PTHREADS)
LDIRT	= $(YFILES:%.y=%.tab.?)

default:	$(CMDTARGET)

include $(BUILDRULES)

pmlogextract:	$(OBJECTS)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

gram.tab.h gram.tab.c:	gram.y

lex.o gram.tab.o:	gram.tab.h

default_pcp:	default

install_pcp:	install
