#
# Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
# Copyright (c) 2019 Red Hat.
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

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM = statsd
DOMAIN = STATSD
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT = $(IAM)_init
PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)

RAGELTARGET = parser-ragel
RFILES = $(RAGELTARGET).c
CFILES = \
	aggregators.c \
	aggregator-metric-counter.c \
	aggregator-metric-duration.c \
	aggregator-metric-duration-exact.c \
	aggregator-metric-duration-hdr.c \
	aggregator-metric-gauge.c \
	aggregator-metric-labels.c \
	aggregator-metrics.c \
	aggregator-stats.c \
	config-reader.c	\
	dict-callbacks.c \
	network-listener.c \
	parsers.c \
	parsers-utils.c \
	parser-basic.c \
	pmda-callbacks.c \
	pmdastatsd.c \
	utils.c

HFILES = $(shell echo *.h)

LLDLIBS = $(LIB_FOR_HDR_HISTOGRAM) $(LIB_FOR_CHAN)
LLDLIBS += $(LIB_FOR_PTHREADS) $(LIB_FOR_MATH)
LLDLIBS += $(PCP_WEBLIB)
LCFLAGS = $(INVISIBILITY)
#LCFLAGS += -Wextra -MMD -MP
LDIRT = domain.h *.log $(RFILES)

VERSION_SCRIPT = export

default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_STATSD)" "true"
build-me: $(CMDTARGET) $(LIBTARGET)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 644 root_statsd $(PMDADIR)/root_statsd
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)

CFILES += $(RFILES)

$(RFILES): $(RAGELTARGET).rl
	$(RAGEL) $<

$(OBJECTS): $(RFILES) domain.h
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: $(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_MAKERULE)

check:: $(CFILES) $(HFILES)
	$(CLINT) $^
