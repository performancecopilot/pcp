#
# Copyright (c) 2013-2014 Red Hat.
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
OTHERS  = control stomp rc_pmie pmie2col.sh pmie_check.sh pmie_daily.sh

LSRCFILES = $(OTHERS) crontab.in
LDIRT	= crontab

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmie
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmie/crontab
endif

default:: crontab

default:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install::	default
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_SYSCONF_DIR)/pmie
	$(INSTALL) -m 664 -o $(PCP_USER) -g $(PCP_GROUP) control $(PCP_PMIECONTROL_PATH)
	$(INSTALL) -m 755 pmie_check.sh $(PCP_BINADM_DIR)/pmie_check
	$(INSTALL) -m 755 pmie_daily.sh $(PCP_BINADM_DIR)/pmie_daily
	$(INSTALL) -m 755 pmie2col.sh $(PCP_BIN_DIR)/pmie2col
	$(INSTALL) -m 755 rc_pmie $(PCP_RC_DIR)/pmie
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmie
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)/pmie
ifeq ($(TARGET_OS),linux)
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
endif
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

crontab: crontab.in
	$(SED) -e 's;@user@;'$(CRONTAB_USER)';' -e 's;@path@;'$(PCP_BINADM_DIR)';g' $< > $@
