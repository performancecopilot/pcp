TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmtime
WRAPPER = $(COMMAND).sh
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h console.h kmtimearch.h kmtimelive.h \
	  seealsodialog.h showboundsdialog.h timelord.h timezone.h
SOURCES = aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp main.cpp

LSRCFILES = $(PROJECT).in $(QRCFILE) $(UIFILES) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).IN
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER)

ICONLINKS = play_off.png play_on.png fastfwd_off.png fastfwd_on.png \
	fastback_off.png fastback_on.png back_off.png back_on.png \
	stepfwd_off.png stepfwd_on.png stepback_off.png stepback_on.png \
	stop_off.png stop_on.png aboutpcp.png aboutqt.png aboutkmtime.png \
	whatsthis.png edit-clear.png internet-web-browser.png kmtime.png \
	$(ICNFILE)

default: iconlinks wrappers
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
MACBUILD = $(COMMAND).app/Contents
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
wrappers: $(WRAPPER)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).IN
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
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
