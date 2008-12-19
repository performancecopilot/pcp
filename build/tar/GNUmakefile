#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LDIRT = $(wildcard kmchart*.tar.gz)

default install install-dev install-lib:

.PHONY: pack_kmchart
pack_kmchart :
	@BINTAR=$(PKG_NAME)-$(PKG_VERSION).tar.gz; \
	HERE=`pwd`; cd $${DIST_ROOT:-/}; \
	$(SORT) -u $$DIST_MANIFEST | $(AWK) ' \
		$$1 == "f" { printf (".%s\n", $$6); } \
		$$1 == "d" { next; } \
		$$1 == "l" { printf (".%s\n", $$3); }' \
	| $(TAR) -T - -cf - | $(ZIP) --best > $$HERE/$(BINTAR)
	@echo Wrote: `pwd`/$(BINTAR)

include $(BUILDRULES)
