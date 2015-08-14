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

CMDTARGET = pcp-atop
DISTARGET = $(PCP_BINADM_DIR)/$(CMDTARGET)
DISTLINKS = pcp-atopsar
LLDFLAGS = -L$(TOPDIR)/src/libpcp_gui/src
LLDLIBS = $(PCP_GUILIB) $(LIB_FOR_NCURSES) $(LIB_FOR_MATH)
LCFLAGS = $(NCURSES_CFLAGS)
GENMAPS = $(shell echo *.map | sed -e 's/\.map/\.h/g')
LDIRT = pmgenmap.sh $(GENMAPS)

CFILES = \
	atop.c \
	atopsar.c \
	deviate.c \
	ifprop.c \
	parseable.c \
	photoproc.c \
	photosyst.c \
	procdbase.c \
	showgeneric.c \
	showlinux.c \
	showprocs.c \
	showsys.c \
	modules.c \
	various.c \
	version.c

HFILES = \
	atop.h \
	ifprop.h \
	netstats.h \
	parseable.h \
	photoproc.h \
	photosyst.h \
	showgeneric.h \
	showlinux.h

MAN_PAGES = pcp-atop.1 pcp-atoprc.5 pcp-atopsar.1

default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_NCURSES)" "true"
build-me: $(CMDTARGET) $(DISTLINKS)

install: default
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
	@for link in $(DISTLINKS); do \
	    $(INSTALL) -S $(DISTARGET) $(PCP_BINADM_DIR)/$$link; \
	done
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(DISTLINKS):	$(CMDTARGET)
	@$(LN_S) $(CMDTARGET) $@

pmgenmap.sh:
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
		$(TOPDIR)/src/pmgenmap/pmgenmap.sh >pmgenmap.sh

procmetrics.h:	pmgenmap.sh procmetrics.map
	$(PMGENMAP) procmetrics.map > procmetrics.h

systmetrics.h:	pmgenmap.sh systmetrics.map
	$(PMGENMAP) systmetrics.map > systmetrics.h

ifpropmetrics.h:	pmgenmap.sh ifpropmetrics.map
	$(PMGENMAP) ifpropmetrics.map > ifpropmetrics.h

hostmetrics.h:	pmgenmap.sh hostmetrics.map
	$(PMGENMAP) hostmetrics.map > hostmetrics.h

photoproc.o:	procmetrics.h
photosyst.o:	systmetrics.h
ifprop.o:	ifpropmetrics.h
various.o:	hostmetrics.h
