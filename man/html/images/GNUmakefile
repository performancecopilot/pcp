TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

ICOFILES = $(shell echo *.ico)
PNGFILES = $(shell echo *.png)
SVGFILES = $(shell echo *.svg)
LSRCFILES = $(ICOFILES) $(PNGFILES) $(SVGFILES)
ICONLINKS = pcpicon.png pmcharticon.png pmtimeicon.png stepfwd_on.png system-search.png
INSTFILES = $(shell echo $(ICONLINKS) $(ICOFILES) $(PNGFILES) | tr ' ' '\n' | sort -u)
LDIRT = $(ICONLINKS)

default: iconlinks

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PCP_BOOKS_DIR)/html/images
	$(INSTALL) -m 644 $(INSTFILES) $(PCP_BOOKS_DIR)/html/images

default_pcp : default

install_pcp : install
