#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
TREEROOT = $(shell cd ${TOPDIR}; pwd)
include $(TOPDIR)/src/include/builddefs

SPEC = pcp-gui.spec
LDIRT = *.rpm $(SPEC) rpmmacros

LSRCFILES = macros.template fedora.spec $(SPEC).in

default install install-dev install-lib:

# Generate a binary rpm file
pack-pcp-gui : rpmmacros $(SPEC)
	DEFS=`grep '^--define' rpmmacros`; \
	eval $(RPMBUILD) -ba $$DEFS --clean $(SPEC)
	rm -f $(SPEC) $(TOPDIR)/install.manifest $(TOPDIR)/files.rpm rpmmacros

rpmmacros : macros.template
	@TREEROOT=`cd ${TOPDIR}; pwd` TARDIST=$$TREEROOT/build/tar; \
	for d in RPM SRPM SPEC; do \
	    eval D=\$$$${d}DIR; \
	    if [ -z "$$D" -o ! -d "$$D" ] ; then \
		eval $${d}DIR=$$TREEROOT/build/rpm; \
	    fi; \
	done; \
	sed -e "s|%topdir%|$${TREEROOT}|g" \
	    -e "s|@topdir@|$${TREEROOT}|g" \
	    -e "s|@tardir@|$${TARDIST}|g" \
	    -e "s|@rpmsdir@|$${RPMDIR}|g" \
	    -e "s|@srpmsdir@|$${SRPMDIR}|"g \
	    -e "s|@specdir@|$${SPECDIR}|"g \
	    -e '/^%/s/[ \t]*$$/'\''/' \
	    -e '/^%/s/^%/--define '\''/' < $< > $@

# Generate the rpm specfile format file list from the install-sh manifest
rpmfiles rpmfiles-doc:
	$(SORT) -u $$DIST_MANIFEST | $(AWK) > $@ '\
$$1 == "d" { printf ("%%%%dir %%%%attr(%s,%s,%s) %s\n", $$2, $$3, $$4, $$5); } \
$$1 == "f" { if (match ($$6, "$(PKG_MAN_DIR)") || \
                match ($$6, "$(PKG_DOC_DIR)")) \
                printf ("%%%%doc "); \
            if (match ($$6, "$(PCP_VAR_DIR)/config")) { \
                printf ("%%%%config(noreplace) "); \
            } \
            if (match ($$6, "$(PKG_MAN_DIR)")) \
                printf ("%%%%attr(%s,%s,%s) %s*\n", $$2, $$3, $$4, $$6); \
            else \
                printf ("%%%%attr(%s,%s,%s) %s\n", $$2, $$3, $$4, $$6); } \
$$1 == "l" { if (match ($$3, "$(PKG_MAN_DIR)") || \
                match ($$3, "$(PKG_DOC_DIR)")) \
                printf ("%%%%doc "); \
            if (match ($$3, "$(PKG_MAN_DIR)")) \
                printf ("%%%%attr(0777,root,root) %s*\n", $$3); \
            else \
                printf ("%%%%attr(0777,root,root) %s\n", $$3); }'

pcp-gui.spec: pcp-gui.spec.in
	@echo Generating $@ from $?; \
	$(SED) \
	    -e's|@pkg_name@|$(PKG_NAME)|g' \
	    -e's|@pkg_version@|$(PKG_VERSION)|g' \
	    -e's|@pkg_release@|$(PKG_RELEASE)|g' \
	    -e's|@pkg_distribution@|$(PKG_DISTRIBUTION)|g' \
	    -e's|@build_root@|$(DIST_ROOT)|g' \
	    -e'/^BuildRoot: *$$/d' \
	    -e's|@make@|$(MAKE)|g' < $? > $@

include $(BUILDRULES)
