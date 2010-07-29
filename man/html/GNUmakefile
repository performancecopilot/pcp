TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
CONFFILES = pmchart.adp pmtime.adp
CONFIGURE = pmchart.adp.in pmtime.adp.in
LSRCFILES = $(HTMLPAGES) $(CONFIGURE)
SUBDIRS = images diskmodel

default: $(HTMLPAGES)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)/html
	$(INSTALL) -m 644 $(CONFFILES) $(HTMLPAGES) $(PKG_HTML_DIR)/html
	$(SUBDIRS_MAKERULE)
