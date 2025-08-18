TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

ICOFILES = $(shell echo *.ico)
PNGFILES = $(shell echo *.png)
SVGFILES = $(shell echo *.svg)
LSRCFILES = $(ICOFILES) $(PNGFILES) $(SVGFILES)
ICONLINKS = pcpicon.png pcpicon-light.png pmcharticon.png pmtimeicon.png stepfwd_on.png system-search.png vector-containers.png
INSTFILES = $(shell echo $(ICONLINKS) $(ICOFILES) $(PNGFILES) | tr ' ' '\n' | sort -u)
LDIRT = $(ICONLINKS)

default:
	@for l in $(ICONLINKS) ; do \
	    if [ ! -L $$l -a ! -f $$l ] ; then \
		$(LN_S) $(TOPDIR)/images/$$l $$l ; \
	    fi \
	done

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PCP_HTML_DIR)/images
	$(INSTALL) -m 644 $(INSTFILES) $(PCP_HTML_DIR)/images

default_pcp : default

install_pcp : install
