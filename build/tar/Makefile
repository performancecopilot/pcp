#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

BINTAR=$(PKG_NAME)-$(PKG_VERSION).tar.gz
LDIRT = *.gz

default install install-dev install-lib:

include $(BUILDRULES)

dist : default
	@HERE=`pwd`; cd $${DIST_ROOT:-/}; \
	$(SORT) -u $$HERE/../bin-manifest | $(AWK) ' \
		$$1 == "f" { printf (".%s\n", $$6); } \
		$$1 == "d" { next; } \
		$$1 == "l" { printf (".%s\n", $$3); }' \
	| $(TAR) -T - -cf - | $(ZIP) --best > $$HERE/$(BINTAR)
	@echo Wrote: `pwd`/$(BINTAR)
