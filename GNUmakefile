#
# Copyright (c) 2012-2022 Red Hat.
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
DOCFILES = README.md INSTALL.md CHANGELOG VERSION.pcp
CONFFILES = pcp.lsm
LDIRT = config.cache config.status config.log files.rpm \
	autom4te.cache install.manifest install.tmpfiles \
	debug*.list devel_files libs_files conf_files \
	base_files.rpm libs_files.rpm devel_files.rpm \
	perl-pcp*.list* python-pcp*.list* python3-pcp*.list* \
	tmpfiles.init.setup
LDIRDIRT = pcp-[0-9]*.[0-9]*.[0-9]*  pcp-*-[0-9]*.[0-9]*.[0-9]*

SUBDIRS = vendor src
ifneq ($(TARGET_OS),mingw)
SUBDIRS += qa
endif
SUBDIRS += man images build debian

default :: default_pcp

pcp : default_pcp

default_pcp : $(CONFIGURE_GENERATED) tmpfiles.init.setup
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
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)
ifneq "$(findstring $(TARGET_OS),darwin mingw)" ""
	$(INSTALL) -m 755 -d $(PCP_RC_DIR)
	$(INSTALL) -m 755 -d $(PCP_SASLCONF_DIR)
	$(INSTALL) -m 755 -d $(PCP_BIN_DIR)
	$(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_LIBADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_PMDASADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_LIB_DIR)
	$(INSTALL) -m 755 -d $(PCP_LIB_DIR)/pkgconfig
	$(INSTALL) -m 755 -d $(PCP_MAN_DIR)
	$(INSTALL) -m 755 -d $(PCP_MAN_DIR)/man1
	$(INSTALL) -m 755 -d $(PCP_MAN_DIR)/man3
	$(INSTALL) -m 755 -d $(PCP_MAN_DIR)/man5
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_TMP_DIR)
	# this works if PCP_RUN_DIR is persistent
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_RUN_DIR)
	# this works if PCP_RUN_DIR (and friends) are within a tmpfs that
	# is mounted empty on re-boot and managed by systemd-tmpfiles(8)
	$(INSTALL) -m 644 tmpfiles.init.setup /usr/lib/tmpfiles.d/pcp-reboot-init.conf
	$(INSTALL) -m 755 -d $(PCP_SYSCONFIG_DIR)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/labels
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/labels/optional
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmchart
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmieconf
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmlogconf
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmcheck
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/lib
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/examples
	$(INSTALL) -m 755 -d $(PCP_INC_DIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/pmns
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmieconf
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogconf
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_VAR_DIR)/config/pmda
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PCP_LOG_DIR)
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

tmpfiles.init.setup:	tmpfiles.init.setup.in
	sed < $< > $@ \
	    -e "s@PCP_RUN_DIR@$(PCP_RUN_DIR)@" \
	    -e "s@PCP_LOG_DIR@$(PCP_LOG_DIR)@" \
	    -e "s/PCP_GROUP/$(PCP_GROUP)/" \
	    -e "s/PCP_USER/$(PCP_USER)/" \
