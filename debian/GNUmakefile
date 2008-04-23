TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = changelog compat control copyright rules

default:

include $(BUILDRULES)

install: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PKG_DOC_DIR)/changelog.Debian
endif
