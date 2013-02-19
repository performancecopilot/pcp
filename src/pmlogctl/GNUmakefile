#!gmake
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

OTHERS	= pmlogger.config pmnewlog.sh pmlogger_daily.sh pmlogger_check.sh \
	pmlogger_merge.sh control config.sar config.base GNUmakefile.install

LSRCFILES = $(OTHERS) crontab.IN
LDIRT	= crontab control.install

# Beware ... spaces at the end of these lines cause the install lines to break
_confdir = $(PCP_SYSCONF_DIR)/pmlogger
_bindir  = $(PCP_BINADM_DIR)

default: $(OTHERS) crontab control.install GNUmakefile.install.xxx

crontab : crontab.IN
	$(SED) -e '/\# .*/b' -e 's;PCP_BINADM_DIR;$(PCP_BINADM_DIR);g' < $< > $@

control.install: control
	$(SED) -e's;/var/adm/pcplog;PCP_LOG_DIR/pmlogger;' $< > $@

include $(BUILDRULES)

install: default crontab
	$(INSTALL) -m 755 -d `dirname $(PCP_PMLOGGERCONTROL_PATH)`
	$(INSTALL) -m 644 control.install $(PCP_PMLOGGERCONTROL_PATH)
	$(INSTALL) -m 644 config.sar config.base crontab $(_confdir)
	$(INSTALL) -m 644 pmlogger.config $(_confdir)/config.default
	$(INSTALL) -m 644 GNUmakefile.install.xxx $(_confdir)/Makefile
	$(INSTALL) -m 755 pmnewlog.sh $(_bindir)/pmnewlog
	$(INSTALL) -m 755 pmlogger_daily.sh $(_bindir)/pmlogger_daily
	$(INSTALL) -m 755 pmlogger_check.sh $(_bindir)/pmlogger_check
	$(INSTALL) -m 755 pmlogger_merge.sh $(_bindir)/pmlogger_merge

default_pcp : default

install_pcp : install

GNUmakefile.install.xxx:	GNUmakefile.install
	sed -e "s;<CPP>;$(PCP_BINADM_DIR)/pmcpp -D$(TARGET_OS);" <GNUmakefile.install >GNUmakefile.install.xxx
