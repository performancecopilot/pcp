#
# Copyright (c) 2013-2015 Red Hat.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src
LDIRT	= *.log

default : $(SUBDIRS) pmcd.service
	$(SUBDIRS_MAKERULE)

install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d `dirname $(PCP_PMCDOPTIONS_PATH)`
	$(INSTALL) -m 644 pmcd.options $(PCP_PMCDOPTIONS_PATH)
	$(INSTALL) -m 755 -d `dirname $(PCP_PMCDRCLOCAL_PATH)`
	$(INSTALL) -m 755 rc_local $(PCP_PMCDRCLOCAL_PATH)
	$(INSTALL) -m 755 rc_pmcd $(PCP_RC_DIR)/pmcd
	$(INSTALL) -m 755 rc_pcp $(PCP_RC_DIR)/pcp
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmcd.service $(PCP_SYSTEMDUNIT_DIR)/pmcd.service
endif
	$(INSTALL) -m 644 pmdaproc.sh $(PCP_SHARE_DIR)/lib/pmdaproc.sh
	$(INSTALL) -m 644 rc-proc.sh $(PCP_SHARE_DIR)/lib/rc-proc.sh
	$(INSTALL) -m 644 rc-proc.sh.minimal $(PCP_SHARE_DIR)/lib/rc-proc.sh.minimal
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_LOG_DIR)/pmcd
	$(INSTALL) -m 700 -d $(PCP_TMP_DIR)/pmcd
ifneq ($(PACKAGE_DISTRIBUTION),debian)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmcd
endif
	$(INSTALL) -m 644 pmcd.defaults $(PCP_SYSCONFIG_DIR)/pmcd
	$(INSTALL) -m 644 sasl2.conf $(PCP_SASLCONF_DIR)/pmcd.conf

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmcd.service: pmcd.service.in
	$(SED) -e 's;@path@;'$(PCP_RC_DIR)';' $< > $@
