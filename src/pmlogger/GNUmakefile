#
# Copyright (c) 2013-2022 Red Hat.
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
OTHERS	= pmnewlog.sh control rc_pmlogger rc_wrapper \
	  pmlogger_daily.sh pmlogger_check.sh \
	  pmlogger_janitor.sh pmlogger_merge.sh \
	  pmlogger_daily_report.sh pmlogger_rewrite.sh \
	  pmlogger_farm.sh pmlogger_farm_check.service \
	  pmlogger_farm_check.timer pmlogger_farm_check.service \
	  pmlogger.pmcheck
GENSCRIPT = crontab crontab.daily_report pmlogger.service \
	  pmlogger_daily.service pmlogger_check.service \
	  pmlogger_farm.service pmlogger_farm_check.service
LDIRT	= $(GENSCRIPT)

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmlogger
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmlogger/crontab
endif
CRONTAB_DAILY_REPORT_PATH = $(CRONTAB_PATH)-daily-report

default: $(GENSCRIPT) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_VAR_DIR)/config/pmlogger
	$(INSTALL) -m 644 pmlogger_farm.defaults $(PCP_SYSCONFIG_DIR)/pmlogger_farm
	$(INSTALL) -m 644 pmlogger.defaults $(PCP_SYSCONFIG_DIR)/pmlogger
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/zeroconf
	$(INSTALL) -m 644 pmlogger.zeroconf $(PCP_SHARE_DIR)/zeroconf/pmlogger
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmlogger
	$(INSTALL) -m 755 -d $(PCP_PMLOGGERCONTROL_PATH).d
	$(INSTALL) -m 644 control $(PCP_PMLOGGERCONTROL_PATH)
	$(INSTALL) -m 644 control.local $(PCP_PMLOGGERCONTROL_PATH).d/local
	$(INSTALL) -m 755 pmnewlog.sh $(PCP_BINADM_DIR)/pmnewlog$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_daily.sh $(PCP_BINADM_DIR)/pmlogger_daily$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_check.sh $(PCP_BINADM_DIR)/pmlogger_check$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_janitor.sh $(PCP_BINADM_DIR)/pmlogger_janitor$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_farm.sh $(PCP_BINADM_DIR)/pmlogger_farm$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_merge.sh $(PCP_BINADM_DIR)/pmlogger_merge$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_rewrite.sh $(PCP_BINADM_DIR)/pmlogger_rewrite$(SHELLSUFFIX)
	$(INSTALL) -m 755 rc_wrapper $(PCP_RC_DIR)/pmlogger
	$(INSTALL) -m 755 rc_pmlogger $(PCP_SYSCONF_DIR)/pmlogger/rc
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmlogger.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger.service
	$(INSTALL) -m 644 pmlogger_farm.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger_farm.service
	$(INSTALL) -m 644 pmlogger_farm_check.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger_farm_check.service
	$(INSTALL) -m 644 pmlogger_farm_check.timer $(PCP_SYSTEMDUNIT_DIR)/pmlogger_farm_check.timer
	$(INSTALL) -m 644 pmlogger_daily.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger_daily.service
	$(INSTALL) -m 644 pmlogger_daily.timer $(PCP_SYSTEMDUNIT_DIR)/pmlogger_daily.timer
	$(INSTALL) -m 644 pmlogger_check.service $(PCP_SYSTEMDUNIT_DIR)/pmlogger_check.service
	$(INSTALL) -m 644 pmlogger_check.timer $(PCP_SYSTEMDUNIT_DIR)/pmlogger_check.timer
	$(INSTALL) -m 644 pmlogger_timers.sysconfig $(PCP_SYSCONFIG_DIR)/pmlogger_timers
else
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)
	$(INSTALL) -m 644 crontab.daily_report $(CRONTAB_DAILY_REPORT_PATH)
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmlogger
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)/pmlogger
	$(INSTALL) -m 644 -t $(PCP_SHARE_DIR)/lib/utilproc.sh utilproc.sh $(PCP_LIBADM_DIR)/utilproc.sh
	$(INSTALL) -m 755 pmlogger_daily_report.sh $(PCP_BINADM_DIR)/pmlogger_daily_report$(SHELLSUFFIX)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_SA_DIR)
	$(INSTALL) -m 755 pmlogger.pmcheck $(PCP_SHARE_DIR)/lib/pmcheck/pmlogger

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmlogger.service : pmlogger.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_SYSCONF_DIR@;'$(PCP_SYSCONF_DIR)';' \
	    -e 's;@PCP_RUN_DIR@;'$(PCP_RUN_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	# END

pmlogger_farm.service : pmlogger_farm.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmlogger_farm_check.service : pmlogger_farm_check.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_BIN_DIR@;'$(PCP_BIN_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmlogger_daily.service : pmlogger_daily.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmlogger_check.service : pmlogger_check.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

crontab : crontab.in
	$(SED) <$< > $@ \
	    -e 's;@user@;'$(CRONTAB_USER)';' \
	    -e 's;@path@;'$(PCP_BINADM_DIR)';' \
	# END

crontab.daily_report : crontab.daily_report.in
	$(SED) <$< > $@ \
	    -e 's;@user@;'$(CRONTAB_USER)';'
	    -e 's;@path@;'$(PCP_BINADM_DIR)';'
	    -e 's;@sapath@;'$(PCP_SA_DIR)';' \
	# END
