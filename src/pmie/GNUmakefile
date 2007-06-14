#! gmake
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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
# 
# $Id: GNUmakefile,v 2.56 2007/02/21 04:44:52 kimbrr Exp $
#

TOPDIR	= ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS	= src examples

LSRCFILES = control etc_init.d_pmie pmie_check.sh crontab config.default \
	stomp.install pmie2col
LDIRT	= control.install config.default.install crontab.install

CFG_DIR	= $(PCP_VAR_DIR)/config/pmie

default:: control.install config.default.install crontab.install

default:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install::	default
	$(INSTALL) -m 755 -d $(CFG_DIR)
	$(INSTALL) -m 644 crontab.install $(CFG_DIR)/crontab
	$(INSTALL) -m 644 config.default.install $(CFG_DIR)/config.default
	$(INSTALL) -m 644 control.install $(CFG_DIR)/control
	$(INSTALL) -m 644 stomp.install $(CFG_DIR)/stomp
	$(INSTALL) -m 755 etc_init.d_pmie $(PCP_RC_DIR)/pmie
	$(INSTALL) -m 755 pmie_check.sh $(PCP_BINADM_DIR)/pmie_check
	$(INSTALL) -m 755 pmie2col $(PCP_BIN_DIR)/pmie2col

include $(BUILDRULES)

default_pcp : default

install_pcp : install

control.install: control
	sed -e's;/var/adm/pmielog/;PCP_LOG_DIR/pmie/;' -e 's;/var/pcp;'$(PCP_VAR_DIR)';g' $< > $@

config.default.install: config.default
	sed -e's;/var/adm/pmielog/;PCP_LOG_DIR/pmie/;' -e 's;/var/pcp;'$(PCP_VAR_DIR)';g' $< > $@

crontab.install: crontab
	sed -e's;/usr/pcp/bin;'$(PCP_BINADM_DIR)';g' $< > $@
