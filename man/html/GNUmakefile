TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
OTHERPAGES = pcpdoc.css
CONFFILES = pcpdoc.adp
CONFIGURE = pcpdoc.adp.in
LSRCFILES = $(HTMLPAGES) $(OTHERPAGES) $(CONFIGURE)
SUBDIRS = images \
	  cpuperf \
	  diskmodel \
	  diskperf \
	  importdata \
	  pmie \
	  pmview \

default: $(HTMLPAGES) $(OTHERPAGES)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)/html
	$(INSTALL) -m 644 $(CONFFILES) $(HTMLPAGES) $(OTHERPAGES) $(PKG_HTML_DIR)/html
	$(SUBDIRS_MAKERULE)
