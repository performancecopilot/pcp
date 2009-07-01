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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

SPEC = pcp.spec
LDIRT += *-[0-9]*.*.rpm $(SPEC) rpmmacros 

PERLFILES = pcp-pmda-perl.desc pcp-logsummary-perl.desc pcp-mmv-perl.desc
LSRCFILES = macros.template pcp.spec.in $(PERLFILES)

default install :

# Blow the spec away after rpm has finished its work - it IS
# going to make sure it'll be rebuild next time we come here
pack_pcp : rpmmacros $(SPEC)
	DEFS=`grep '^--define' rpmmacros`; \
	eval $(RPMPROG) -ba $$DEFS \
		--target $(TARGET_CPU)-$(TARGET_VENDOR)-$(TARGET_OS) \
		--clean $(SPEC)
	rm -f $(SPEC) $(TOPDIR)/install.manifest $(TOPDIR)/files.rpm \
		rpmmacros 
ifneq "$(CPANFLUTE)" ""
	$(CPANFLUTE) --email="$(PACKAGE_BUILDER)" \
		--noperlreqs --release=1 --outdir=$(TOPDIR)/build/rpm \
		--descfile=pcp-pmda-perl.desc --buildrequires=pcp \
		--buildall $(TOPDIR)/src/cpan/PMDA/PCP-PMDA-*.tar.gz
	$(CPANFLUTE) --email="$(PACKAGE_BUILDER)" \
		--noperlreqs --release=1 --outdir=$(TOPDIR)/build/rpm \
		--descfile=pcp-logsummary-perl.desc --buildrequires=pcp \
		--buildall $(TOPDIR)/src/cpan/LogSummary/PCP-LogSummary-*.tar.gz
	$(CPANFLUTE) --email="$(PACKAGE_BUILDER)" \
		--noperlreqs --release=1 --outdir=$(TOPDIR)/build/rpm \
		--descfile=pcp-mmv-perl.desc --buildrequires=pcp \
		--buildall $(TOPDIR)/src/cpan/MMV/PCP-MMV-*.tar.gz
endif

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
	    -e '/^%/s/^%/--define '\''/' < $< > $@

pcp.spec: pcp.spec.in
	@echo Generating $@ from $?; \
	sed -e's|@package_sgirelease@|$(SGIRELEASENUMBER)|g' \
	    -e's|@sgi_chroot_build@|$(SGI_CHROOT_BUILD)|g' \
	    -e's|@sgi_issp_build@|$(SGI_ISSP_BUILD)|g' \
	    -e's|@have_ibdev@|$(HAVE_IBDEV)|g' \
	    -e's|@package_name@|$(PACKAGE_NAME)|g' \
	    -e's|@package_version@|$(PACKAGE_VERSION)|g' \
	    -e's|@package_release@|$(PACKAGE_BUILD)|g' \
	    -e's|@package_distribution@|$(PACKAGE_DISTRIBUTION)|g' \
	    -e's|@package_builder@|$(PACKAGE_BUILDER)|g' \
	    -e"s|@build_root@|$${DIST_ROOT}|g" \
	    -e's|@pcp_var_dir@|$(PCP_VAR_DIR)|g' \
	    -e's|@pcp_share_dir@|$(PCP_SHARE_DIR)|g' \
	    -e's|@pcp_log_dir@|$(PCP_LOG_DIR)|g' \
	    -e's|@pcp_run_dir@|$(PCP_RUN_DIR)|g' \
	    -e's|@pcp_tmp_dir@|$(PCP_TMP_DIR)|g' \
	    -e's|@make@|$(PCP_MAKE_PROG)|g' < $? > $@

include $(BUILDRULES)

