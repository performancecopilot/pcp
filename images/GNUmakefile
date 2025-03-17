TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

ifeq "$(ENABLE_QT)" "true"
DESKTOP_ICONS = pmchart.png pmtime.png pmview.png
endif
HTMLDOC_ICONS = pcpicon.png pmcharticon.png pmtimeicon.png pmviewicon.png

SVGFILES = $(shell echo *.svg)
PNGFILES = $(shell echo *.svg | sed -e 's/\.svg/.png/g')
PNGFILES += $(DESKTOP_ICONS)
ICNFILES = $(shell echo *.icns *.ico)

APPFILES = $(shell echo $(DESKTOP_ICONS) | sed -e 's/\.png//g')
APPSIZES = 16 22 24 32 48 64 128 256
HICOLORDIR = $(shell echo $(PCP_SHARE_DIR) | sed -e 's;/pcp$$;;')/icons/hicolor

LDIRT =

# use checked in files by default, so no default build rule
default:

.NOTPARALLEL:
.PHONEY:	hicolor.done rebuild_icons
hicolor.done:	$(DESKTOP_ICONS)

# rebuild desktop icons ... requires inkscape to be installed
#
rebuild_icons:
ifneq ($(INKSCAPE),)
	@rm -f hicolor-*
	for sz in $(APPSIZES); do \
	    for app in $(APPFILES); do \
		$(INKSCAPE) -o hicolor-$${sz}x$${sz}-$${app}.png -w $${sz} $${app}.svg; \
	    done; \
	done
else
	@echo "Error: need to install inkscape to rebuild desktop icons"
	false
endif

#%.png:	%.svg
%.png:	
	$(INKSCAPE) -z -o $@ $<

include $(BUILDRULES)

install: default
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 755 -d $(PCP_ICONS_DIR)
	$(INSTALL) -m 644 $(DESKTOP_ICONS) $(PCP_ICONS_DIR)
endif
ifeq ($(HAVE_HICOLOR_ICONS),true)
	$(INSTALL) -m 755 -d $(HICOLORDIR)
	for sz in $(APPSIZES); do \
	    $(INSTALL) -m 755 -d $(HICOLORDIR)/$${sz}x$${sz}; \
	    $(INSTALL) -m 755 -d $(HICOLORDIR)/$${sz}x$${sz}/apps; \
	    for app in $(APPFILES); do \
		icon=$(HICOLORDIR)/$${sz}x$${sz}/apps/$${app}.png; \
		$(INSTALL) -m 644 hicolor-$${sz}x$${sz}-$${app}.png $${icon}; \
	    done; \
	done
	$(INSTALL) -m 755 -d $(HICOLORDIR)/scalable
	$(INSTALL) -m 755 -d $(HICOLORDIR)/scalable/apps
	for app in $(APPFILES); do \
	    $(INSTALL) -m 644 $${app}.svg $(HICOLORDIR)/scalable/apps/$${app}.svg; \
	done
endif

default_pcp : default

install_pcp : install
