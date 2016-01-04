TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

DESKTOP_ICONS = pmchart.png pmtime.png
HTMLDOC_ICONS = pcpicon.png pmcharticon.png pmtimeicon.png

INKSCAPE = inkscape
SVGFILES = $(shell echo *.svg)
PNGFILES = $(shell echo *.svg | sed -e 's/\.svg/.png/g')
PNGFILES += $(DESKTOP_ICONS)
ICNFILES = $(shell echo *.icns *.ico)

# use checked in files by default, so no default build rule
default:

#%.png:	%.svg
%.png:	
	$(INKSCAPE) -z -e $@ $<

include $(BUILDRULES)

install: default
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 -d $(PCP_ICONS_DIR)
	$(INSTALL) -m 644 $(DESKTOP_ICONS) $(PCP_ICONS_DIR)
endif

default_pcp : default

install_pcp : install
