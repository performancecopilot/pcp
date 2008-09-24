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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = dbpmda$(EXECSUFFIX)
CFILES	= dbpmda.c dso.c util.c pmda.c
HFILES	= lex.h dbpmda.h
LFILES	= lex.l
YFILES	= gram.y

LDIRT	= *.log foo.* gram.h $(YFILES:%.y=%.tab.?) $(LFILES:%.l=%.c)
LLDLIBS	= -lpcp $(LIB_FOR_DLOPEN)

default:	$(CMDTARGET)

include $(BUILDRULES)

$(CMDTARGET):	$(OBJECTS)

install:	$(CMDTARGET) 
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

dbpmda.o:	gram.h
lex.o:		gram.tab.h

gram.tab.h:	gram.y

gram.h:		gram.tab.h
	rm -f $@ && $(LN_S) $< $@

default_pcp:	default

install_pcp:	install
