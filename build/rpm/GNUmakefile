#
# Copyright (c) 2012-2022 Red Hat.
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
	eval $(RPMPROG) -ba $$DEFS --clean $(SPEC)
	@if [ -n "$$RPM_SIGN_KEYID" ]; then \
	    echo "Signing packages with key: $$RPM_SIGN_KEYID"; \
	    RPMDIR=`sed -n -e "s,'$$,/,g" -e 's/.*_rpmdir *//p' < rpmmacros`; \
	    RPMS=`rpmspec -q --rpms $(SPEC) | \
		sed -e "s,^,$${RPMDIR},g" -e 's/$$/.rpm/g'`; \
	    rpmsign --addsign --key-id=$${RPM_SIGN_KEYID} $${RPMS}; \
	fi
	rm -f $(TOPDIR)/install.manifest $(TOPDIR)/files.rpm rpmmacros

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
	    -e's|@pmda_amdgpu@|$(PMDA_AMDGPU)|g' \
	    -e's|@pmda_bcc@|$(PMDA_BCC)|g' \
	    -e's|@pmda_bpf@|$(PMDA_BPF)|g' \
	    -e's|@pmda_bpftrace@|$(PMDA_BPFTRACE)|g' \
	    -e's|@pmda_gfs2@|$(PMDA_GFS2)|g' \
	    -e's|@pmda_statsd@|$(PMDA_STATSD)|g' \
	    -e's|@pmda_systemd@|$(PMDA_SYSTEMD)|g' \
	    -e's|@pmda_infiniband@|$(PMDA_INFINIBAND)|g' \
	    -e's|@pmda_perfevent@|$(PMDA_PERFEVENT)|g' \
	    -e's|@pmda_openmetrics@|$(PMDA_OPENMETRICS)|g' \
	    -e's|@pmda_libvirt@|$(PMDA_LIBVIRT)|g' \
	    -e's|@pmda_lio@|$(PMDA_LIO)|g' \
	    -e's|@pmda_json@|$(PMDA_JSON)|g' \
	    -e's|@pmda_nginx@|$(PMDA_NGINX)|g' \
	    -e's|@pmda_oracle@|$(PMDA_ORACLE)|g' \
	    -e's|@pmda_mongodb@|$(PMDA_MONGODB)|g' \
	    -e's|@pmda_postgresql@|$(PMDA_POSTGRESQL)|g' \
	    -e's|@pmda_postfix@|$(PMDA_POSTFIX)|g' \
	    -e's|@pmda_elasticsearch@|$(PMDA_ELASTICSEARCH)|g' \
	    -e's|@pmda_snmp@|$(PMDA_SNMP)|g' \
	    -e's|@pmda_mssql@|$(PMDA_MSSQL)|g' \
	    -e's|@pmda_mysql@|$(PMDA_MYSQL)|g' \
	    -e's|@pmda_activemq@|$(PMDA_ACTIVEMQ)|g' \
	    -e's|@pmda_bind2@|$(PMDA_BIND2)|g' \
	    -e's|@pmda_nutcracker@|$(PMDA_NUTCRACKER)|g' \
	    -e's|@pmda_resctrl@|$(PMDA_RESCTRL)|g' \
		-e's|@pmda_hdb@|$(PMDA_HDB)|g' \
	    -e's|@pcp2arrow@|$(HAVE_PYTHON_PYARROW)|g' \
	    -e's|@pcp2xlsx@|$(HAVE_PYTHON_OPENPYXL)|g' \
	    -e's|@pcp2influxdb@|$(HAVE_PYTHON_INFLUXDB)|g' \
	    -e's|@pcp2elasticsearch@|$(HAVE_PYTHON_REQUESTS)|g' \
	    -e's|@enable_systemd@|$(ENABLE_SYSTEMD)|g' \
	    -e's|@enable_secure@|$(ENABLE_SECURE)|g' \
	    -e's|@enable_probes@|$(ENABLE_PROBES)|g' \
	    -e's|@enable_avahi@|$(ENABLE_AVAHI)|g' \
	    -e's|@enable_dstat@|$(ENABLE_DSTAT)|g' \
	    -e's|@enable_qt@|$(ENABLE_QT)|g' \
	    -e's|@enable_selinux@|$(ENABLE_SELINUX)|g' \
	    -e's|@enable_lzma@|$(ENABLE_LZMA)|g' \
	    -e's|@have_python@|$(HAVE_PYTHON)|g' \
	    -e's|@have_perl@|$(HAVE_PERL)|g' \
	    -e"s|@build_root@|$${DIST_ROOT}|g" \
	    -e's|@pcp_pmcdconf_path@|$(PCP_PMCDCONF_PATH)|g' \
	    -e's|@pcp_systemdsysusers_dir@|$(PCP_SYSTEMDSYSUSERS_DIR)|g' \
	    -e's|@pcp_systemdtmpfiles_dir@|$(PCP_SYSTEMDTMPFILES_DIR)|g' \
	    -e's|@pcp_sysconfig_dir@|$(PCP_SYSCONFIG_DIR)|g' \
	    -e's|@pcp_sysconf_dir@|$(PCP_SYSCONF_DIR)|g' \
	    -e's|@pcp_share_dir@|$(PCP_SHARE_DIR)|g' \
	    -e's|@pcp_log_dir@|$(PCP_LOG_DIR)|g' \
	    -e's|@pcp_sa_dir@|$(PCP_SA_DIR)|g' \
	    -e's|@pcp_var_dir@|$(PCP_VAR_DIR)|g' \
	    -e's|@pcp_etc_dir@|$(PCP_ETC_DIR)|g' \
	    -e's|@pcp_selinux_dir@|$(PCP_SELINUX_DIR)|g' \
	    -e's|@pcp_binadm_dir@|$(PCP_BINADM_DIR)|g' \
	    -e's|@pcp_pmdas_dir@|$(PCP_PMDAS_DIR)|g' \
	    -e's|@qt_version@|$(QT_VERSION)|g' \
	    -e's|@rpm_version@|$(RPM_VERSION)|g' \
	    -e's|@have_manpages@|$(HAVE_MANPAGES)|g' \
	    -e's|@make@|$(PCP_MAKE_PROG)|g' <$? >$@
	./do-maybe-requires

include $(BUILDRULES)

