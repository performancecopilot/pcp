#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LDIRT = $(wildcard pcp-gui*.tar.gz)

default install install-dev install-lib:

.PHONY: pack-pcp-gui
pack-pcp-gui :
	@ BINTAR=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE).tar.gz; HERE=`pwd`; \
	cd $${DIST_ROOT:-/}; \
	$(SORT) -u $$DIST_MANIFEST \
	| $(AWK) ' \
		$$1 == "f" { printf (".%s\n", $$6); } \
		$$1 == "d" { next; } \
		$$1 == "l" { printf (".%s\n", $$3); }' \
	| $(TAR) -T - -cf - \
	| $(ZIP) --best > $$HERE/$$BINTAR; \
	echo Wrote: $$HERE/$$BINTAR

include $(BUILDRULES)
