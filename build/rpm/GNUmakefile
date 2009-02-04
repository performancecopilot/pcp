#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
TREEROOT = $(shell cd ${TOPDIR}; pwd)
include $(TOPDIR)/src/include/builddefs

SPECF = $(PKG_NAME).spec
LDIRT = *.rpm $(SPECF) rpmmacros rpmfiles* rpm-*.rc

LSRCFILES = macros.template $(SPECF).in rpm-2.rc.template

default install install-dev install-lib:

# Generate a binary rpm file
pack-pcp-gui : default $(SPECF) rpm-$(RPM_VERSION).rc
	$(RPMBUILD) -ba --rcfile ./rpm-$(RPM_VERSION).rc $(SPECF)

# Because rpm prior to v.2.90 does not support macros and old style config
# is not supported by rpm v.3, we have to resort to such ugly hacks
ifneq ($(RPM_VERSION),2)
rpm-$(RPM_VERSION).rc : rpmmacros
	@$(SED) -e '/^macrofiles:/s|~/.rpmmacros|rpmmacros|' </usr/lib/rpm/rpmrc >$@

rpmmacros : macros.template
	@$(SED) -e 's|%topdir%|$(TREEROOT)|g' < $< > $@
else
rpm-2.rc: rpm-2.rc.template
	@$(SED) -e 's|%topdir%|$(TOPDIR)|g' < $< > $@
endif

# Generate the rpm specfile format file list from the install-sh manifest
rpmfiles rpmfiles-dev rpmfiles-lib:
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

${SPECF} : ${SPECF}.in
	$(SED) -e's|@pkg_name@|$(PKG_NAME)|g' \
	    -e's|@pkg_version@|$(PKG_VERSION)|g' \
	    -e's|@pkg_release@|$(PKG_RELEASE)|g' \
	    -e's|@pkg_distribution@|$(PKG_DISTRIBUTION)|g' \
	    -e's|@build_root@|$(DIST_ROOT)|g' \
	    -e'/^BuildRoot: *$$/d' \
	    -e's|@make@|$(MAKE)|g' < $< > $@

include $(BUILDRULES)
