#
# Copyright (c) 2013 Red Hat.
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

LSRCFILES = $(OTHERS) crontab.in
LDIRT	= crontab

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmlogger
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmlogger/crontab
endif

default: $(SUBDIRS) crontab
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install: $(SUBDIRS) crontab
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_SYSCONF_DIR)/pmlogger
	$(INSTALL) -m 664 -o $(PCP_USER) -g $(PCP_GROUP) control $(PCP_PMLOGGERCONTROL_PATH)
	$(INSTALL) -m 755 pmnewlog.sh $(PCP_BINADM_DIR)/pmnewlog$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_daily.sh $(PCP_BINADM_DIR)/pmlogger_daily$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_check.sh $(PCP_BINADM_DIR)/pmlogger_check$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogger_merge.sh $(PCP_BINADM_DIR)/pmlogger_merge$(SHELLSUFFIX)
	$(INSTALL) -m 755 pmlogmv.sh $(PCP_BIN_DIR)/pmlogmv$(SHELLSUFFIX)
	$(INSTALL) -m 755 rc_pmlogger $(PCP_RC_DIR)/pmlogger
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmlogger
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)/pmlogger
ifeq ($(TARGET_OS),linux)
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
endif
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)

default_pcp : default

install_pcp : install

crontab : crontab.in
	$(SED) -e 's;@user@;'$(CRONTAB_USER)';' -e 's;@path@;'$(PCP_BINADM_DIR)';' $< > $@
