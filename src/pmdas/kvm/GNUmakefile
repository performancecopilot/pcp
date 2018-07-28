#
# Copyright (c) 2018 Fujitsu.
# Copyright (c) 2018 Red Hat.
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

TOPDIR     = ../../..
include $(TOPDIR)/src/include/builddefs

IAM        = kvm
DOMAIN     = KVM
PMDADIR	   = $(PCP_PMDAS_DIR)/$(IAM)
CMDTARGET  = pmda$(IAM)
LIBTARGET  = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT   = $(IAM)_init
CONF_LINE  = "kvm	95	pipe	binary		$(PMDADIR)/$(CMDTARGET) -d 95"

CFILES	   = kvmstat.c
HFILES	   = kvmstat.h
SCRIPTS	   = Install Remove

VERSION_SCRIPT	= exports
LLDLIBS    = $(PCP_PMDALIB)
DFILES	   = help
LSRCFILES  = kvm_root root $(DFILES) $(SCRIPTS)
LDIRT      = domain.h $(VERSION_SCRIPT)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET)
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 root pmns help domain.h $(IAM).conf $(PMDADIR)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

domain.h    : ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

kvmstat.o: domain.h kvmstat.h $(VERSION_SCRIPT)
