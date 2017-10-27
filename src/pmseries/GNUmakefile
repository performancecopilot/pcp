#
# Copyright (c) 2017 Red Hat.
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

CFILES	= load.c redis.c sha1.c util.c series.c pmseries.c
HFILES	= load.h redis.h sha1.h util.h series.h
XFILES	= sha1.c sha1.h
YFILES	= query_parser.y

CMDTARGET = pmseries$(EXECSUFFIX)
LLDLIBS	= $(PCPLIB) $(LIB_FOR_MATH) $(LIB_FOR_HIREDIS)
LDIRT	= $(XFILES) $(CMDTARGET) $(YFILES:%.y=%.tab.?)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_HIREDIS)" "true"
build-me:	$(XFILES) $(CMDTARGET)
install_pcp install:
install:	default
	#$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
else
build-me:
install_pcp install:
endif

$(XFILES):
	$(LN_S) $(TOPDIR)/src/external/$@ .

.NOTPARALLEL:
query_parser.tab.h query_parser.tab.c: query_parser.y series.h
	$(YACC) -d -b `basename $< .y` $<
