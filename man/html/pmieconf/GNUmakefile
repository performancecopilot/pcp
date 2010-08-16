TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

BUNDLE = pmieconf
SCRIPT = piggie
LSRCFILES = $(SCRIPT)

default: 

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_DEMOS_DIR)/$(BUNDLE)
	$(INSTALL) -m 644 $(SCRIPT) $(PKG_DEMOS_DIR)/$(BUNDLE)/$(SCRIPT)
