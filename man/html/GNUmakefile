TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
CONFFILES = kmchart.adp kmtime.adp
CONFIGURE = kmchart.adp.in kmtime.adp.in
LSRCFILES = $(HTMLPAGES) $(CONFIGURE)
SUBDIRS = images

default: $(HTMLPAGES)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html
	$(INSTALL) -m 644 $(CONFFILES) $(HTMLPAGES) $(PKG_DOC_DIR)/html
	$(SUBDIRS_MAKERULE)
