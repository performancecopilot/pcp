TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
CONFIGURE = kmchart.adp.in kmtime.adp.in
ASSISTANT = kmchart.adp kmtime.adp
LSRCFILES = $(HTMLPAGES) $(CONFIGURE)
SUBDIRS = images
LDIRT = $(ASSISTANT)

default: $(HTMLPAGES) $(ASSISTANT)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html
	$(INSTALL) -m 644 $(ASSISTANT) $(HTMLPAGES) $(PKG_DOC_DIR)/html
	$(SUBDIRS_MAKERULE)
