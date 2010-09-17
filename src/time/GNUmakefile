TOPDIR = ../..
COMMAND = pmtime
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
RCFILE  = $(COMMAND).rc
ICOFILE  = $(COMMAND).ico
ICNFILE = $(COMMAND).icns
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h console.h pmtimearch.h pmtimelive.h \
	  seealsodialog.h showboundsdialog.h timelord.h timezone.h
SOURCES = aboutdialog.cpp console.cpp pmtimearch.cpp pmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp main.cpp
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(QRCFILE) $(RCFILE) $(UIFILES) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).IN
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER)

ICONLINKS = play_off.png play_on.png fastfwd_off.png fastfwd_on.png \
	fastback_off.png fastback_on.png back_off.png back_on.png \
	stepfwd_off.png stepfwd_on.png stepback_off.png stepback_on.png \
	stop_off.png stop_on.png aboutpcp.png aboutqt.png aboutpmtime.png \
	whatsthis.png edit-clear.png internet-web-browser.png pmtime.png \
	$(ICNFILE) $(ICOFILE)

default: iconlinks wrappers
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
wrappers: $(WRAPPER)
copies: Info.plist PkgInfo
else
wrappers:
copies:
endif

$(WRAPPER): $(WRAPPER).IN
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@
Info.plist: default
	@ cp -f $(MACBUILD)/Info.plist Info.plist
PkgInfo: default
	@ cp -f $(MACBUILD)/PkgInfo PkgInfo

install: default copies
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
ifeq ($(WINDOW),win)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d /Library
	$(INSTALL) -m 755 -d /Library/PCP
	$(INSTALL) -m 755 -d /Library/PCP/$(COMMAND).app
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)
	$(INSTALL) -m 644 Info.plist $(PKG_MAC_DIR)/Info.plist
	$(INSTALL) -m 644 PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/MacOS
	$(INSTALL) -m 755 $(BINARY) $(PKG_MAC_DIR)/MacOS/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/Resources
	$(INSTALL) -m 644 $(ICNFILE) $(PKG_MAC_DIR)/Resources/$(ICNFILE)
endif
