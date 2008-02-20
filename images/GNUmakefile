TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

DESKTOP_ICONS = kmchart.png kmtime.png
HTMLDOC_ICONS = kmcharticon.png kmtimeicon.png

INKSCAPE = inkscape
SVGFILES = $(shell echo *.svg)
PNGFILES = $(shell echo *.svg | sed -e 's/\.svg/.png/g')
PNGFILES += $(DESKTOP_ICONS)
ICNFILES = $(shell echo *.icns)

LSRCFILES = $(SVGFILES) $(PNGFILES) $(ICNFILES) $(HTMLDOC_ICONS)

# use checked in files by default, so no default build rule
default:

pngfiles: $(PNGFILES)

%.png: %.svg
	$(INKSCAPE) -z -e $@ $<

include $(BUILDRULES)

install: default
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 -d $(PKG_ICON_DIR)
	$(INSTALL) -m 644 $(DESKTOP_ICONS) $(PKG_ICON_DIR)
endif
