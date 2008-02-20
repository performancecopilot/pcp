#! gmake
#
# Copyright (c) 2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
# $Id: GNUmakefile,v 2.70 2007/03/30 09:30:06 kimbrr.bonnie.engr.sgi.com Exp $
#

ifneq (,)
This makefile requires GNU Make.
endif

ifeq ($(shell [ -f $(WORKAREA)/linuxmeister/release ] && echo 1),1)

include $(WORKAREA)/linuxmeister/release

ifeq ($(PROJECT),mangrove)
include GNUissp
else
ifeq ($(SUBPRO),)
include VERSION.pro

build-sgi clean::
	$(MAKE) -f GNUmakefile $@ SUBPRO=GNUissp PCP_DIST=$(PRO_DISTRIBUTION)
	$(MAKE) -f GNUmakefile $@ SUBPRO=GNUpropack
else
include $(SUBPRO)
endif
endif
else

TOPDIR = .
-include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

LICFILES = COPYING
DOCFILES = README INSTALL CHANGELOG VERSION.pcp
LSRCFILES = configure configure.in Makepkgs install-sh $(DOCFILES) $(LICFILES) \
	    config.guess config.sub sgiReleaseNumber pcp.lsm.in
LDIRT = config.cache autom4te.cache config.status config.log config.done \
	files.rpm pro_files.rpm pcp.spec pcp.lsm \
	pcp-$(PACKAGE_MAJOR).$(PACKAGE_MINOR).$(PACKAGE_REVISION) \
	pcp-pro-$(PACKAGE_MAJOR).$(PACKAGE_MINOR).$(PACKAGE_REVISION) \
	pcp-sgi-$(PACKAGE_MAJOR).$(PACKAGE_MINOR).$(PACKAGE_REVISION) \
	root-*/include root-*/lib root-*/*.rpm root-*/default_pro \
	autom4te.cache install.manifest install_pro.manifest

SUBDIRS = src man build debian
ifeq "$(MAKECMDGOALS)" "clobber"
ifeq ($(shell [ -d qa ] && echo 1),1)
SUBDIRS	+= qa
endif
endif

default :: default_pcp

pcp : default_pcp

default_pcp : config.done 
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d $@ || exit $$?; \
	    fi; \
	done

install :: default_pcp install_pcp

pack_pcp : default_pcp
	$(MAKE) -C build $@

install_pcp :  default_pcp
	# install the common directories _once_
	# TODO automagic needed here to avoid installing directories like
	# /usr/bin or /usr/lib or /etc that really belong to some other
	# package
	# for Linux, this once comes from the chkconfig package
	# $(INSTALL) -m 755 -d $(PCP_RC_DIR)
	# for Linux, this group comes from the filesystem package
	# $(INSTALL) -m 755 -d $(PCP_BIN_DIR)
	# $(INSTALL) -m 755 -d $(PCP_LIB_DIR)
	# $(INSTALL) -m 755 -d $(PCP_MAN_DIR)
	$(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/lib
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/examples
	$(INSTALL) -m 755 -d $(PCP_INC_DIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmieconf
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogger
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/pmns
	$(INSTALL) -m 755 -d $(PCP_PMDAS_DIR)
	$(INSTALL) -m 755 -d $(PCP_LOG_DIR)
	$(INSTALL) -m 755 -d $(PCP_RUN_DIR)
	$(INSTALL) -m 755 -d $(PCP_DOC_DIR)
	$(INSTALL) -m 755 -d $(PCP_DEMOS_DIR)
	#
	@for d in `echo $(SUBDIRS)`; do \
	    if test -d "$$d" ; then \
		echo === $$d ===; \
		$(MAKE) -C $$d $@ || exit $$?; \
	    fi; \
	done
ifneq "$(PACKAGE_DISTRIBUTION)" "debian"
	$(INSTALL) -m 644 $(LICFILES) $(PCP_DOC_DIR)/$(LICFILES)
endif
	$(INSTALL) -m 644 pcp.lsm $(DOCFILES) $(PCP_DOC_DIR)

ifdef BUILDRULES
include $(BUILDRULES)
else
# if src/include/builddefs doesn't exist, we are pristine (hence also clean)
realclean clean clobber clean-lbs clean-sgi:
	@true
endif

endif # -f $(WORKAREA)/linuxmeister/release

config.done: configure
	./configure && touch $@

configure : configure.in
	rm -fr config.cache autom4te.cache
	autoconf
