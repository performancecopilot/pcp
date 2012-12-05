#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
# 

TOPDIR	= ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS	= src examples

LSRCFILES = control rc_pmie pmie_check.sh crontab config.default \
	stomp.install pmie2col pmie_daily.sh
LDIRT	= control.install config.default.install crontab.install

CFG_DIR	= $(PCP_VAR_DIR)/config/pmie

default:: control.install config.default.install crontab.install

default:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install::	default
	$(INSTALL) -m 755 -d `dirname $(PCP_PMIECONTROL_PATH)`
	$(INSTALL) -m 644 control.install $(PCP_PMIECONTROL_PATH)
	$(INSTALL) -m 755 -d $(CFG_DIR)
	$(INSTALL) -m 644 stomp.install $(CFG_DIR)/stomp
	$(INSTALL) -m 644 crontab.install $(CFG_DIR)/crontab
	$(INSTALL) -m 644 config.default.install $(CFG_DIR)/config.default
	$(INSTALL) -m 755 pmie_check.sh $(PCP_BINADM_DIR)/pmie_check
	$(INSTALL) -m 755 pmie_daily.sh $(PCP_BINADM_DIR)/pmie_daily
	$(INSTALL) -m 755 pmie2col $(PCP_BIN_DIR)/pmie2col
	$(INSTALL) -m 755 rc_pmie $(PCP_RC_DIR)/pmie
ifeq "$(findstring $(PCP_TMP_DIR),/var/tmp /tmp)" ""
	$(INSTALL) -m 1777 -d $(PCP_TMP_DIR)/pmie
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install

control.install: control
	sed -e's;/var/adm/pmielog/;PCP_LOG_DIR/pmie/;' -e 's;/var/pcp;'$(PCP_VAR_DIR)';g' $< > $@

config.default.install: config.default
	sed -e's;/var/adm/pmielog/;PCP_LOG_DIR/pmie/;' -e 's;/var/pcp;'$(PCP_VAR_DIR)';g' $< > $@

crontab.install: crontab
	sed -e's;/usr/pcp/bin;'$(PCP_BINADM_DIR)';g' $< > $@
