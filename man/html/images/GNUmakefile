TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PNGFILES = $(shell echo *.png)
LSRCFILES = $(PNGFILES)
ICONLINKS = kmcharticon.png kmtimeicon.png
WINDOWLINKS = tutorial1.png tutorial2.png tutorial3.png tutorial4.png \
	tutorial5.png tutorial6.png tutorial7.png tutorial8.png tutorial9.png \
	tutorial10.png tutorial11.png  
INSTFILES = $(ICONLINKS) $(WINDOWLINKS)
LDIRT = $(ICONLINKS) $(WINDOWLINKS)

default: iconlinks windowlinks

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html/images
	$(INSTALL) -m 644 $(INSTFILES) $(PKG_DOC_DIR)/html/images
