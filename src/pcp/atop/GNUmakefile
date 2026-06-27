#
# Copyright (c) 2015,2019-2021,2023 Red Hat.
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
LLDLIBS = $(PCPLIB) -lpcp_import -lpcp_archive $(LIB_FOR_NCURSESW) $(LIB_FOR_MATH)
GENMAPS = $(shell echo *.map | sed -e 's/\.map/\.h/g')
LDIRT = pmgenmap.sh $(DISTLINKS) $(GENMAPS) \
	pcp-atop.service pcp-atop-rotate.service pcp-atop-rotate.timer atop.conf

CFILES = \
	atop.c \
	atopsar.c \
	acctproc.c \
	deviate.c \
	drawbar.c \
	gpucom.c \
	ifprop.c \
	parseable.c \
	photoproc.c \
	netproc.c \
	photosyst.c \
	procdbase.c \
	showgeneric.c \
	showlinux.c \
	showprocs.c \
	showsys.c \
	various.c \
	version.c

HFILES = \
	atop.h \
	gpucom.h \
	ifprop.h \
	netstats.h \
	parseable.h \
	photoproc.h \
	photosyst.h \
	showgeneric.h \
	showlinux.h

MAN_PAGES = pcp-atop.1 pcp-atoprc.5 pcp-atopsar.1

default:	build-me \
	pcp-atop.service pcp-atop-rotate.service pcp-atop-rotate.timer atop.conf

include $(BUILDRULES)

pcp-atop.service: pcp-atop.service.in
	$(SED) \
	    -e 's|@PCP_BINADM_DIR@|$(PCP_BINADM_DIR)|g' \
	    -e 's|@PCP_LOG_DIR@|$(PCP_LOG_DIR)|g' \
	    -e 's|@PCP_SYSCONF_DIR@|$(PCP_SYSCONF_DIR)|g' \
	    $< > $@

pcp-atop-rotate.service: pcp-atop-rotate.service.in
	cp $< $@

pcp-atop-rotate.timer: pcp-atop-rotate.timer.in
	cp $< $@

atop.conf: atop.conf.in
	$(SED) \
	    -e 's|@PCP_LOG_DIR@|$(PCP_LOG_DIR)|g' \
	    -e 's|@PCP_SYSCONF_DIR@|$(PCP_SYSCONF_DIR)|g' \
	    $< > $@

ifeq "$(HAVE_NCURSESW)" "true"
build-me: $(CMDTARGET) $(DISTLINKS)

install: default
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
	@for link in $(DISTLINKS); do \
	    $(INSTALL) -S $(DISTARGET) $(PCP_BINADM_DIR)/$$link; \
	done
	$(INSTALL) -m 644 pcp-atop.service $(PCP_SYSTEMDUNIT_DIR)/pcp-atop.service
	$(INSTALL) -m 644 pcp-atop-rotate.service $(PCP_SYSTEMDUNIT_DIR)/pcp-atop-rotate.service
	$(INSTALL) -m 644 pcp-atop-rotate.timer $(PCP_SYSTEMDUNIT_DIR)/pcp-atop-rotate.timer
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/atop
	$(INSTALL) -m 644 atop.conf $(PCP_SYSCONF_DIR)/atop/atop.conf
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(DISTLINKS):	$(CMDTARGET)
	@$(LN_S) -f $(CMDTARGET) $@

pmgenmap.sh:
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
		$(TOPDIR)/src/pmgenmap/pmgenmap.sh >pmgenmap.sh

procmetrics.h:	pmgenmap.sh procmetrics.map
	$(PMGENMAP) procmetrics.map > procmetrics.h

acctmetrics.h:	pmgenmap.sh acctmetrics.map
	$(PMGENMAP) acctmetrics.map > acctmetrics.h

netprocmetrics.h:	pmgenmap.sh netprocmetrics.map
	$(PMGENMAP) netprocmetrics.map > netprocmetrics.h

systmetrics.h:	pmgenmap.sh systmetrics.map
	$(PMGENMAP) systmetrics.map > systmetrics.h

ifpropmetrics.h:	pmgenmap.sh ifpropmetrics.map
	$(PMGENMAP) ifpropmetrics.map > ifpropmetrics.h

hostmetrics.h:	pmgenmap.sh hostmetrics.map
	$(PMGENMAP) hostmetrics.map > hostmetrics.h

gpumetrics.h:	pmgenmap.sh gpumetrics.map
	$(PMGENMAP) gpumetrics.map > gpumetrics.h

photoproc.o:	procmetrics.h
netproc.o:	netprocmetrics.h
photosyst.o:	systmetrics.h
ifprop.o:	ifpropmetrics.h
various.o:	hostmetrics.h
acctproc.o:	acctmetrics.h
gpucom.o:	gpumetrics.h

atop.o atopsar.o various.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check::	$(MAN_PAGES)
	$(MANLINT) $^
