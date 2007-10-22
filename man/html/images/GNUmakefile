TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PNGFILES = $(shell echo *.png)
LSRCFILES = $(PNGFILES) $(CONFIGURE)
ICONLINKS = kmcharticon.png kmtimeicon.png
LDIRT = $(ICONLINKS)

default: iconlinks

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html/images
	$(INSTALL) -m 644 $(PNGFILES) $(PKG_DOC_DIR)/html/images
