#
# Copyright (c) 2013,2020 Red Hat.
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

CFILES = pmfind.c
HFILES = groups.h
CMDTARGET = pmfind$(EXECSUFFIX)
LDIRT = crontab pmfind.service

ifeq "$(HAVE_LIBUV)" "true"
CFILES += source.c
LLDLIBS = $(PCP_WEBLIB)
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1
else
CFILES += nosource.c
LLDLIBS = $(PCPLIB)
endif

ifeq ($(TARGET_OS),linux)
CRONTAB_USER = $(PCP_USER)
CRONTAB_PATH = $(PCP_ETC_DIR)/cron.d/pcp-pmfind
else
CRONTAB_USER =
CRONTAB_PATH = $(PCP_SYSCONF_DIR)/pmfind/crontab
endif

default : $(CMDTARGET) crontab pmfind.service

include $(BUILDRULES)

install : default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 pmfind_check.sh $(PCP_BINADM_DIR)/pmfind_check$(SHELLSUFFIX)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)/pmfind
	$(INSTALL) -m 644 pmfind.sysconfig $(PCP_SYSCONFIG_DIR)/pmfind
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmfind
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmfind.service $(PCP_SYSTEMDUNIT_DIR)/pmfind.service
	$(INSTALL) -m 644 pmfind.timer $(PCP_SYSTEMDUNIT_DIR)/pmfind.timer
else
	$(INSTALL) -m 755 -d `dirname $(CRONTAB_PATH)`
	$(INSTALL) -m 644 crontab $(CRONTAB_PATH)
endif

default_pcp : default

install_pcp : install

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h

pmfind.service : pmfind.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_SYSCONFIG_DIR@;'$(PCP_SYSCONFIG_DIR)';' \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_VAR_DIR@;'$(PCP_VAR_DIR)';' \
	    -e 's;@SD_SERVICE_TYPE@;'$(SD_SERVICE_TYPE)';' \
	# END

crontab: crontab.in
	$(SED) -e 's;@user@;'$(CRONTAB_USER)';' -e 's;@path@;'$(PCP_BINADM_DIR)';g' $< > $@
