TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

PCP_GUI_FILES = 
PCP_DOC_FILES = pcp-doc.dirs pcp-doc.install doc-base
PCP_DOC_DIR = $(PKG_DOC_DIR)/../pcp-doc
DOC_BASE_DIR = /usr/share/doc-base

SUBDIRS = source
LSRCFILES = changelog compat control copyright rules \
	$(PCP_GUI_FILES) $(PCP_DOC_FILES)

default:

include $(BUILDRULES)

install: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PKG_DOC_DIR)/changelog.Debian
endif

install-doc: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCP_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCP_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCP_DOC_DIR)/changelog.Debian
	$(INSTALL) -m 755 -d $(DOC_BASE_DIR)
	$(INSTALL) -m 644 doc-base $(DOC_BASE_DIR)/pcp-doc
endif
