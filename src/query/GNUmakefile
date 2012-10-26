TOPDIR = ../..
COMMAND = pmquery
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
XMLFILE = $(COMMAND).info
HEADERS = pmquery.h
SOURCES = $(HEADERS:.h=.cpp) main.cpp
SCRIPTS = pmconfirm.sh pmmessage.sh
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(QRCFILE) $(SCRIPTS) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).IN pmconfirm.sh.IN pmmessage.sh.IN $(XMLFILE).in
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER) $(SCRIPTS) $(XMLFILE)

ICONLINKS = $(ICNFILE) dialog-archive.png dialog-error.png dialog-host.png \
	    dialog-information.png dialog-question.png dialog-warning.png

default: iconlinks wrappers
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
PKG_SUB_DIR = $(PKG_MAC_DIR)/MacOS
wrappers: $(WRAPPER) $(SCRIPTS)
else
PKG_SUB_DIR = $(PKG_BIN_DIR)
wrappers: $(WRAPPER) $(SCRIPTS)
endif

$(WRAPPER): $(WRAPPER).IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmmessage.sh: pmmessage.sh.IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmconfirm.sh: pmconfirm.sh.IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifneq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
	$(INSTALL) -m 755 pmconfirm.sh $(PKG_BIN_DIR)/pmconfirm
	$(INSTALL) -m 755 pmmessage.sh $(PKG_BIN_DIR)/pmmessage
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PKG_BIN_DIR)/$(COMMAND)
	$(call INSTALL_DIRECTORY_HIERARCHY,$(PKG_MAC_DIR),/Library)
	$(INSTALL) -m 644 $(XMLFILE) $(PKG_MAC_DIR)/Info.plist
	$(INSTALL) -m 644 $(MACBUILD)/PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/MacOS
	$(call INSTALL_QT_FRAMEWORKS,$(BINARY))
	$(INSTALL) -m 755 $(BINARY) $(PKG_MAC_DIR)/MacOS/$(COMMAND)
	rm $(BINARY)
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/Resources
	$(INSTALL) -m 644 $(ICNFILE) $(PKG_MAC_DIR)/Resources/$(ICNFILE)
	$(call INSTALL_QT_RESOURCES,$(PKG_MAC_DIR)/Resources)
endif
