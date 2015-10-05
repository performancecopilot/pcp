#
# Copyright (c) 2012-2015 Red Hat.
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
-include ../GNUlocaldefs
-include ./GNUlocaldefs

SPEC = pcp.spec
LDIRT += *-[0-9]*.*.rpm $(SPEC) rpmmacros

PERLFILES = pcp-pmda-perl.desc pcp-logsummary-perl.desc pcp-mmv-perl.desc
RPM_VERSION=$(shell ./get_rpm_version)

default install install_pcp default_pcp :

# Blow the spec away after rpm has finished its work - it IS
# going to make sure it'll be rebuild next time we come here
# We used to override --target=FOO, but it's better to let the
# local build distro defaults do their jobs.
pack_pcp : rpmmacros $(SPEC)
	DEFS=`grep '^--define' rpmmacros`; \
	eval $(RPMPROG) -ba $$DEFS \
		--clean $(SPEC)
	rm -f $(TOPDIR)/install.manifest $(TOPDIR)/files.rpm \
		rpmmacros 

rpmmacros : macros.template
	@TREEROOT=`cd ${TOPDIR}; pwd`; \
	for d in RPM SRPM; do \
	    eval D=\$$$${d}DIR; \
	    if [ -z "$$D" -o ! -d "$$D" ] ; then \
		eval $${d}DIR=$$TREEROOT/build/rpm; \
	    fi; \
	done; \
	sed -e "s|%topdir%|$${TREEROOT}|g" \
	    -e "s|@rpmsdir@|$${RPMDIR}|g" \
	    -e "s|@srpmsdir@|$${SRPMDIR}|"g \
	    -e '/^%/s/[ \t]*$$/'\''/' \
	    -e '/^%/s/^%/--define '\''/' <$< >$@

pcp.spec: pcp.spec.in
	@echo Generating $@ from $?; \
	sed -e's|@package_sgirelease@|$(SGIRELEASENUMBER)|g' \
	    -e's|@sgi_chroot_build@|$(SGI_CHROOT_BUILD)|g' \
	    -e's|@sgi_issp_build@|$(SGI_ISSP_BUILD)|g' \
	    -e's|@package_name@|$(PACKAGE_NAME)|g' \
	    -e's|@package_version@|$(PACKAGE_VERSION)|g' \
	    -e's|@package_release@|$(PACKAGE_BUILD)|g' \
	    -e's|@package_configure@|$(PACKAGE_CONFIGURE)|g' \
	    -e's|@package_distribution@|$(PACKAGE_DISTRIBUTION)|g' \
	    -e's|@pmda_rpm@|$(PMDA_RPM)|g' \
	    -e's|@pmda_papi@|$(PMDA_PAPI)|g' \
	    -e's|@pmda_systemd@|$(PMDA_SYSTEMD)|g' \
	    -e's|@pmda_infiniband@|$(PMDA_INFINIBAND)|g' \
	    -e's|@pmda_perfevent@|$(PMDA_PERFEVENT)|g' \
	    -e's|@pmda_json@|$(PMDA_JSON)|g' \
	    -e's|@enable_systemd@|$(ENABLE_SYSTEMD)|g' \
	    -e's|@enable_secure@|$(ENABLE_SECURE)|g' \
	    -e's|@enable_probes@|$(ENABLE_PROBES)|g' \
	    -e's|@enable_avahi@|$(ENABLE_AVAHI)|g' \
	    -e's|@enable_qt@|$(ENABLE_QT)|g' \
	    -e's|@enable_python2@|$(ENABLE_PYTHON2)|g' \
	    -e's|@enable_python3@|$(ENABLE_PYTHON3)|g' \
	    -e's|@enable_manager@|$(ENABLE_MANAGER)|g' \
	    -e's|@enable_webapi@|$(ENABLE_WEBAPI)|g' \
	    -e's|@have_libmicrohttpd@|$(have_libmicrohttpd)|g' \
	    -e's|@have_cairo@|$(have_cairo)|g' \
	    -e's|@have_python@|$(HAVE_PYTHON)|g' \
	    -e's|@have_vector@|$(HAVE_VECTOR)|g' \
	    -e's|@have_webjs@|$(HAVE_WEBJS)|g' \
	    -e"s|@build_root@|$${DIST_ROOT}|g" \
	    -e's|@pcp_sysconf_dir@|$(PCP_SYSCONF_DIR)|g' \
	    -e's|@pcp_share_dir@|$(PCP_SHARE_DIR)|g' \
	    -e's|@pcp_log_dir@|$(PCP_LOG_DIR)|g' \
	    -e's|@pcp_var_dir@|$(PCP_VAR_DIR)|g' \
	    -e's|@pcp_etc_dir@|$(PCP_ETC_DIR)|g' \
	    -e's|@rpm_version@|$(RPM_VERSION)|g' \
	    -e's|@make@|$(PCP_MAKE_PROG)|g' <$? >$@

include $(BUILDRULES)

