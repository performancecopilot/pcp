#
# Copyright (c) 2018 Fujitsu.
# Copyright (c) 2018,2020 Red Hat.
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
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite
CMDTARGET  = pmda$(IAM)
LIBTARGET  = pmda_$(IAM).$(DSOSUFFIX)
PMDAINIT   = $(IAM)_init
CONF_LINE  = "kvm	95	pipe	binary		$(PMDATMPDIR)/$(CMDTARGET) -d 95"

CFILES	   = kvmstat.c
HFILES	   = kvmstat.h
SCRIPTS	   = Install Remove Upgrade

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LLDLIBS    = $(PCP_PMDALIB)
DFILES	   = help
LDIRT      = $(HELPTARGETS) domain.h $(VERSION_SCRIPT)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

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
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help help.dir help.pag root root_kvm $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/kvm.conf kvm.conf $(PMDACONFIG)/kvm.conf
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_kvm root_kvm $(PCP_PMNSADM_DIR)/root_kvm
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/kvm_fixups.conf kvm_fixups.conf $(REWRITEDIR)/kvm_fixups.conf
	@$(INSTALL_MAN)
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

check:: $(MAN_PAGES)
	$(MANLINT) $^
