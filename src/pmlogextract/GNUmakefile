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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES	= pmlogextract.c logio.c error.c metriclist.c
HFILES	= logger.h
LFILES  = lex.l

CMDTARGET = pmlogextract$(EXECSUFFIX)

LLDLIBS	= -lpcp -lm
LDIRT	= $(YFILES:%.y=%.tab.?)

default:	$(CMDTARGET)

ifeq "$(TARGET_OS)" "interix"
# use these rules for platforms where yacc/bison is broken
CFILES	+= gram.tab.c
HFILES	+= gram.tab.h
gram.tab.c:	gram.save.c
	cp gram.save.c gram.tab.c
gram.tab.h:	gram.save.h
	cp gram.save.h gram.tab.h
LDIRT	+= gram.tab.c gram.tab.h
LSRCFILES	+= gram.y
else
# do it properly
YFILES = gram.y
gram.tab.h:	gram.y
LSRCFILES	+= gram.save.c gram.save.h
endif

include $(BUILDRULES)

pmlogextract:	$(OBJECTS)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

lex.o gram.tab.o:	gram.tab.h

default_pcp:	default

install_pcp:	install
