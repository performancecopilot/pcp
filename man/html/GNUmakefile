TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
OTHERPAGES = pcpdoc.css mover2pcp
CONFFILES = pmchart.adp pmtime.adp
CONFIGURE = pmchart.adp.in pmtime.adp.in
LSRCFILES = $(HTMLPAGES) $(OTHERPAGES) $(CONFIGURE) template
SUBDIRS = images diskmodel diskperf cpuperf pmie

default: $(HTMLPAGES) $(OTHERPAGES)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)/html
	$(INSTALL) -m 644 $(CONFFILES) $(HTMLPAGES) $(OTHERPAGES) $(PKG_HTML_DIR)/html
	$(SUBDIRS_MAKERULE)

mover2pcp:	refresh_mover2pcp

refresh_mover2pcp:
	@if [ -f ../../../pcp/src/pmimport/examples/mover2pcp ]; \
	then if [ ! -f mover2pcp ]; \
	    then cp ../../../pcp/src/pmimport/examples/mover2pcp mover2pcp; \
		echo "mover2cp: copied"; \
	    elif diff ../../../pcp/src/pmimport/examples/mover2pcp mover2pcp >/dev/null; \
	    then echo "mover2pcp: no change"; \
	    else cp ../../../pcp/src/pmimport/examples/mover2pcp mover2pcp; \
		echo "mover2pcp: updated"; \
	    fi; \
	fi

