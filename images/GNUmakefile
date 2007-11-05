TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

INKSCAPE = inkscape
SVGFILES = $(shell echo *.svg)
PNGFILES = $(shell echo *.svg | sed -e 's/\.svg/.png/g')
PNGFILES += kmcharticon.png kmtimeicon.png
ICNFILES = $(shell echo *.icns)

LSRCFILES = $(SVGFILES) $(PNGFILES) $(ICNFILES)

# use checked in files by default, so no default build rule
default:

pngfiles: $(PNGFILES)

%.png: %.svg
	$(INKSCAPE) -z -e $@ $<

include $(BUILDRULES)

install: default
