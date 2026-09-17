#
# Copyright (c) 2017-2020,2026 Red Hat.
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

default:	build-me

ifeq "$(HAVE_SQLITE3)" "true"
CFILES = pmsearch.c
CMDTARGET = pmsearch$(EXECSUFFIX)
LLDLIBS	= $(PCP_WEBLIB)
GENSCRIPT = crontab pmsearch_daily.service
LDIRT = $(GENSCRIPT)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
endif

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmsearch
ifeq ($(ENABLE_SYSTEMD),true)
CRONTAB_INSTALL = $(PCP_SHARE_DIR)/cron.d/pcp-pmsearch
else
CRONTAB_INSTALL = $(CRONTAB_PATH)
endif
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmsearch/crontab
CRONTAB_INSTALL = $(CRONTAB_PATH)
endif

build-me:	$(CMDTARGET) $(GENSCRIPT)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 pmsearch_daily.sh $(PCP_BINADM_DIR)/pmsearch_daily
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmsearch
	$(INSTALL) -S $(PCP_SYSCONF_DIR)/pmproxy/pmproxy.conf $(PCP_SYSCONF_DIR)/pmsearch/pmsearch.conf
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmsearch_daily.service $(PCP_SYSTEMDUNIT_DIR)/pmsearch_daily.service
	$(INSTALL) -m 644 pmsearch_daily.timer $(PCP_SYSTEMDUNIT_DIR)/pmsearch_daily.timer
endif
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_INSTALL)`
	$(INSTALL) -m 644 crontab $(CRONTAB_INSTALL)

pmsearch_daily.service : pmsearch_daily.service.in
	$(SED) <$< >$@ \
	    -e 's;@CRONTAB_PATH@;'$(CRONTAB_PATH)';' \
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

else
build-me:
install:
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install
