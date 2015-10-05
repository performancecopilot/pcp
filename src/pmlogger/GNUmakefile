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
OTHERS	= pmnewlog.sh control rc_pmlogger \
	  pmlogger_daily.sh pmlogger_check.sh pmlogger_merge.sh pmlogmv.sh
LDIRT	= crontab crontab.docker pmlogger.service

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmlogger
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmlogger/crontab
endif

default:: crontab crontab.docker pmlogger.service

default:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: default
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_VAR_DIR)/config/pmlogger
	$(INSTALL) -m 644 pmlogger.defaults $(PCP_SYSCONFIG_DIR)/pmlogger
ifneq ($(PACKAGE_DISTRIBUTION),debian)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmlogger
endif
	$(INSTALL) -m 755 -d $(PCP_PMLOGGERCONTROL_PATH).d
	$(INSTALL) -m 644 control $(PCP_PMLOGGERCONTROL_PATH)
	$(INSTALL) -m 644 control.local $(PCP_PMLOGGERCONTROL_PATH).d/local
	$(INSTALL) -m 755 pmnewlog.sh $(PCP_BINADM_DIR)/pmnewlog$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_daily.sh $(PCP_BINADM_DIR)/pmlogger_daily$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_check.sh $(PCP_BINADM_DIR)/pmlogger_check$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_merge.sh $(PCP_BINADM_DIR)/pmlogger_merge$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogmv.sh $(PCP_BIN_DIR)/pmlogmv$(SHELLSUFFIX)
	$(INSTALL) -m 755 rc_pmlogger $(PCP_RC_DIR)/pmlogger
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmlogger.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger.service
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmlogger
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)/pmlogger
ifeq ($(TARGET_OS),linux)
	# In a container environment, the rc_pmlogger script installs the
	# Docker version of the crontab into the hosts cron.d directory.
	$(INSTALL) -m 644 crontab.docker $(PCP_VAR_DIR)/config/pmlogger/crontab.docker
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
endif
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmlogger.service : pmlogger.service.in
	$(SED) -e 's;@path@;'$(PCP_RC_DIR)';' $< > $@

crontab : crontab.in
	$(SED) -e 's;@user@;'$(CRONTAB_USER)';' -e 's;@path@;'$(PCP_BINADM_DIR)';' $< > $@

crontab.docker : crontab.docker.in
	# docker crontab runs as root. this may change in the future.
	$(SED) -e 's;@path@;'$(PCP_BINADM_DIR)';' $< > $@
