TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmquery
WRAPPER = $(COMMAND).sh
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
HEADERS = kmquery.h
SOURCES = $(HEADERS:.h=.cpp) main.cpp
SCRIPTS = kmconfirm.sh kmmessage.sh

LSRCFILES = $(PROJECT) $(QRCFILE) $(SCRIPTS) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).IN kmconfirm.sh.IN kmmessage.sh.IN
LDIRDIRT = build $(COMMAND).xcodeproj
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
	$(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
kmmessage.sh: kmmessage.sh.IN
	$(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
kmconfirm.sh: kmconfirm.sh.IN
	$(SED) -e '/\# .*/b' -e 's;PKG_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
	$(INSTALL) -m 755 kmconfirm.sh $(PKG_BIN_DIR)/kmconfirm
	$(INSTALL) -m 755 kmmessage.sh $(PKG_BIN_DIR)/kmmessage
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
