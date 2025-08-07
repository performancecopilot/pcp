#
# Copyright (c) 2014-2019 Red Hat.
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
OTHERS	= rc_pmproxy rc_wrapper
GENSCRIPT = crontab pmproxy.service \
	    pmproxy_daily.service pmproxy_check.service
LDIRT = *.log $(GENSCRIPT)

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmproxy
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmproxy/crontab
endif

default: $(GENSCRIPT) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmproxy
	$(INSTALL) -m 644 pmproxy.conf pmproxy.options $(PCP_SYSCONF_DIR)/pmproxy
	$(INSTALL) -m 644 pmproxy.defaults $(PCP_SYSCONFIG_DIR)/pmproxy
	$(INSTALL) -m 755 rc_wrapper $(PCP_RC_DIR)/pmproxy$(SHELLSUFFIX)
	$(INSTALL) -m 755 rc_pmproxy $(PCP_SYSCONF_DIR)/pmproxy/rc$(SHELLSUFFIX)
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmproxy.service $(PCP_SYSTEMDUNIT_DIR)/pmproxy.service
	$(INSTALL) -m 644 pmproxy_daily.service $(PCP_SYSTEMDUNIT_DIR)/pmproxy_daily.service
	$(INSTALL) -m 644 pmproxy_daily.timer $(PCP_SYSTEMDUNIT_DIR)/pmproxy_daily.timer
	$(INSTALL) -m 644 pmproxy_check.service $(PCP_SYSTEMDUNIT_DIR)/pmproxy_check.service
	$(INSTALL) -m 644 pmproxy_check.timer $(PCP_SYSTEMDUNIT_DIR)/pmproxy_check.timer
	$(INSTALL) -m 644 pmproxy_timers.sysconfig $(PCP_SYSCONFIG_DIR)/pmproxy_timers
else
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmproxy
	$(INSTALL) -m 755 pmproxy.pmcheck $(PCP_SHARE_DIR)/lib/pmcheck/pmproxy$(SHELLSUFFIX)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmproxy.service: pmproxy.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_SYSCONF_DIR@;'$(PCP_SYSCONF_DIR)';' \
	    -e 's;@PCP_RUN_DIR@;'$(PCP_RUN_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	# END

pmproxy_daily.service : pmproxy_daily.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@PCP_GROUP@;'$(PCP_GROUP)';' \
	    -e 's;@PCP_USER@;'$(PCP_USER)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

pmproxy_check.service : pmproxy_check.service.in
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
	$(SED) <$< >$@ \
	    -e 's;@user@;'$(CRONTAB_USER)';' \
	    -e 's;@path@;'$(PCP_BINADM_DIR)';' \
	# END
