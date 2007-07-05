#
# Copyright (c) 2007 Nathan Scott.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/src/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/src/include/builddefs
endif

CONFIGURE = configure src/include/builddefs
LSRCFILES = configure configure.in Makepkgs aclocal.m4 install-sh README VERSION

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* *.gz

SUBDIRS = src m4 images doc man debian build

default: $(CONFIGURE)
ifeq ($(HAVE_BUILDDEFS), no)
	$(MAKE) -C . $@
else
	$(SUBDIRS_MAKERULE)
endif

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:	# if configure hasn't run, nothing to clean
endif

$(CONFIGURE):
	autoconf
	./configure $$LOCAL_CONFIGURE_OPTIONS
	touch .census

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 README $(PKG_DOC_DIR)

install-dev: default
	$(SUBDIRS_MAKERULE)

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs

ifeq ($(HAVE_BUILDDEFS), no)
clobber: realclean
endif
