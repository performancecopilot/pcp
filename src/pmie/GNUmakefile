#
# Copyright (c) 2013-2015,2020-2022 Red Hat.
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

TOPDIR	= ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS	= src examples
OTHERS  = control stomp rc_pmie pmie2col.sh pmie_check.sh pmie_daily.sh \
          pmie_farm.sh pmie_farm_check.service pmie_farm_check.timer
LDIRT	= crontab pmie.service pmie_daily.service pmie_check.service \
          pmie_farm.service pmie_farm_check.service
          

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmie
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmie/crontab
endif

default:: crontab pmie.service pmie_daily.service pmie_check.service \
	  pmie_farm.service pmie_farm_check.service

default:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: default
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_VAR_DIR)/config/pmie
ifneq ($(PACKAGE_DISTRIBUTION),debian)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmie
endif
	$(INSTALL) -m 755 -d $(PCP_PMIECONTROL_PATH).d
	$(INSTALL) -m 644 control $(PCP_PMIECONTROL_PATH)
	$(INSTALL) -m 644 control.local $(PCP_PMIECONTROL_PATH).d/local
	$(INSTALL) -m 755 pmie_check.sh $(PCP_BINADM_DIR)/pmie_check$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmie_farm.sh $(PCP_BINADM_DIR)/pmie_farm$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmie_daily.sh $(PCP_BINADM_DIR)/pmie_daily$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmie2col.sh $(PCP_BIN_DIR)/pmie2col$(SHELLSUFFIX)
	$(INSTALL) -m 755 rc_pmie $(PCP_RC_DIR)/pmie
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmie.service $(PCP_SYSTEMDUNIT_DIR)/pmie.service
	$(INSTALL) -m 644 pmie_daily.timer $(PCP_SYSTEMDUNIT_DIR)/pmie_daily.timer
	$(INSTALL) -m 644 pmie_daily.service $(PCP_SYSTEMDUNIT_DIR)/pmie_daily.service
	$(INSTALL) -m 644 pmie_farm.service $(PCP_SYSTEMDUNIT_DIR)/pmie_farm.service
	$(INSTALL) -m 644 pmie_farm_check.service $(PCP_SYSTEMDUNIT_DIR)/pmie_farm_check.service
	$(INSTALL) -m 644 pmie_farm_check.timer $(PCP_SYSTEMDUNIT_DIR)/pmie_farm_check.timer
	$(INSTALL) -m 644 pmie_check.timer $(PCP_SYSTEMDUNIT_DIR)/pmie_check.timer
	$(INSTALL) -m 644 pmie_check.service $(PCP_SYSTEMDUNIT_DIR)/pmie_check.service
	$(INSTALL) -m 644 pmie_timers.sysconfig $(PCP_SYSCONFIG_DIR)/pmie_timers
else
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmie
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)/pmie

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmie.service : pmie.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_RC_DIR@;'$(PCP_RC_DIR)';' \
	    -e 's;@PCP_RUN_DIR@;'$(PCP_RUN_DIR)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	# END

pmie_farm.service : pmie_farm.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmie_farm_check.service : pmie_farm_check.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_BIN_DIR@;'$(PCP_BIN_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmie_check.service : pmie_check.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmie_daily.service : pmie_daily.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

crontab: crontab.in
	$(SED) -e 's;@user@;'$(CRONTAB_USER)';' -e 's;@path@;'$(PCP_BINADM_DIR)';g' $< > $@
