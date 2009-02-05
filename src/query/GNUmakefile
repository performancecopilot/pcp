TOPDIR = ../..
COMMAND = pmquery
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
HEADERS = pmquery.h
SOURCES = $(HEADERS:.h=.cpp) main.cpp
SCRIPTS = pmconfirm.sh pmmessage.sh
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(QRCFILE) $(SCRIPTS) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).IN pmconfirm.sh.IN pmmessage.sh.IN
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER) $(SCRIPTS)

ICONLINKS = $(ICNFILE) dialog-archive.png dialog-error.png dialog-host.png \
	    dialog-information.png dialog-question.png dialog-warning.png

default: iconlinks wrappers
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
MACBUILD = $(COMMAND).app/Contents
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
PKG_SUB_DIR = $(PKG_MAC_DIR)/MacOS
else
PKG_SUB_DIR = $(PKG_BIN_DIR)
endif

wrappers: $(WRAPPER) $(SCRIPTS)

$(WRAPPER): $(WRAPPER).IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmmessage.sh: pmmessage.sh.IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmconfirm.sh: pmconfirm.sh.IN
	@ $(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
ifeq ($(WINDOW),win)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
	$(INSTALL) -m 755 pmconfirm.sh $(PKG_BIN_DIR)/pmconfirm
	$(INSTALL) -m 755 pmmessage.sh $(PKG_BIN_DIR)/pmmessage
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d /Library
	$(INSTALL) -m 755 -d /Library/PCP
	$(INSTALL) -m 755 -d /Library/PCP/$(COMMAND).app
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)
	$(INSTALL) -m 644 $(MACBUILD)/Info.plist $(PKG_MAC_DIR)/Info.plist
	$(INSTALL) -m 644 $(MACBUILD)/PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/MacOS
	$(INSTALL) -m 755 $(BINARY) $(PKG_MAC_DIR)/MacOS/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/Resources
	$(INSTALL) -m 644 $(MACBUILD)/Resources/$(ICNFILE) $(PKG_MAC_DIR)/Resources/$(ICNFILE)
endif
