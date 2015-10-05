#
# Copyright (c) 2012-2015 Red Hat.
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

ifneq (,)
This makefile requires GNU Make.
endif

TOPDIR = .
-include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CONFIGURE_GENERATED = pcp.lsm \
	$(TOPDIR)/src/include/builddefs \
	$(TOPDIR)/src/include/pcp/platform_defs.h

LICFILES = COPYING
DOCFILES = README INSTALL CHANGELOG VERSION.pcp
CONFFILES = pcp.lsm
LDIRT = config.cache config.status config.log files.rpm pro_files.rpm \
	autom4te.cache install.manifest install_pro.manifest \
	debug*.list devel_files libs_files conf_files \
	base_files.rpm libs_files.rpm devel_files.rpm \
	perl-pcp*.list* python-pcp*.list* python3-pcp*.list*
LDIRDIRT = pcp-[0-9]*.[0-9]*.[0-9]*  pcp-*-[0-9]*.[0-9]*.[0-9]*

SUBDIRS = src
ifneq ($(TARGET_OS),mingw)
SUBDIRS += qa
endif
SUBDIRS += man books images build debian

default :: default_pcp

pcp : default_pcp

default_pcp : $(CONFIGURE_GENERATED)
	+for d in `echo $(SUBDIRS)`; do \
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
ifneq "$(findstring $(TARGET_OS),darwin mingw)" ""
	# for Linux, this one comes from the chkconfig package
	$(INSTALL) -m 755 -d $(PCP_RC_DIR)
	$(INSTALL) -m 755 -d $(PCP_SASLCONF_DIR)
endif
ifeq ($(TARGET_OS),mingw)
	# for Linux, this group comes from the filesystem package
	$(INSTALL) -m 755 -d $(PCP_BIN_DIR)
	$(INSTALL) -m 755 -d $(PCP_LIB_DIR)
	$(INSTALL) -m 755 -d $(PCP_MAN_DIR)
else
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)
ifeq "$(findstring $(PACKAGE_DISTRIBUTION), debian redhat fedora)" ""
	# $PCP_RUN_DIR usually -> /var/run which may be a temporary filesystem
	# and Debian's lintian complains about packages including /var/run/xxx
	# artifacts ... $PCP_RUN_DIR is also conditionally created on the
	# fly in each before use case, so the inclusion in the package is
	# sometimes desirable, but not mandatory
	#
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_RUN_DIR)
endif
	$(INSTALL) -m 755 -d $(PCP_SYSCONFIG_DIR)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)
	$(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/lib
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/examples
	$(INSTALL) -m 755 -d $(PCP_INC_DIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmieconf
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogconf
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_VAR_DIR)/config/pmda
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/pmns
	$(INSTALL) -m 755 -d $(PCP_PMDAS_DIR)
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
	$(INSTALL) -m 755 install-sh $(PCP_BINADM_DIR)/install-sh

ifdef BUILDRULES
include $(BUILDRULES)
else
# if src/include/builddefs doesn't exist, we are pristine (hence also clean)
realclean distclean clean clobber:
	@true
endif

aclocal.m4:
	# older aclocal(1) versions use --acdir but not the current versions
	aclocal --system-acdir=`pwd`/m4 --output=$@

pcp.lsm src/include/builddefs src/include/pcp/platform_defs.h: configure pcp.lsm.in src/include/builddefs.in src/include/pcp/platform_defs.h.in
	@echo Please run ./configure with the appropriate options to generate $@.
	@false
