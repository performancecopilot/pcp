TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CSSFILES = $(shell echo *.css)
HTMLFILES = $(shell echo *.html)
LSRCFILES = $(HTMLFILES) $(CSSFILES)
SUBDIRS = images \
	  cpuperf \
	  diskmodel \
	  diskperf \
	  importdata \
	  pmie \
	  pmview \

default: $(HTMLFILES) $(CSSFILES)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)/html
	$(INSTALL) -m 644 $(HTMLFILES) $(CSSFILES) $(PKG_HTML_DIR)/html
	$(SUBDIRS_MAKERULE)
