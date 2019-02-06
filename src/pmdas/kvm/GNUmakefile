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
SCRIPTS	   = Install Remove Upgrade

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LLDLIBS    = $(PCP_PMDALIB)
DFILES	   = help
LDIRT      = $(HELPTARGETS) domain.h $(VERSION_SCRIPT)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag root root_kvm kvm.conf $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 root_kvm $(PCP_VAR_DIR)/pmns/root_kvm
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

domain.h    : ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(HELPTARGETS) : help
	$(NEWHELP) -n root_kvm -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h
kvmstat.o: kvmstat.h $(VERSION_SCRIPT)
