TOPDIR = ../..
COMMAND = pmtime
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
RCFILE  = $(COMMAND).rc
ICOFILE = $(COMMAND).ico
ICNFILE = $(COMMAND).icns
XMLFILE = $(COMMAND).info
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h console.h pmtime.h pmtimearch.h pmtimelive.h \
	  seealsodialog.h showboundsdialog.h timelord.h timezone.h
SOURCES = aboutdialog.cpp console.cpp pmtime.cpp pmtimearch.cpp pmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp main.cpp
LSRCFILES = $(PROJECT) $(QRCFILE) $(RCFILE) $(UIFILES) $(HEADERS) \
	   $(SOURCES) $(WRAPPER).IN $(XMLFILE).in
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER) $(XMLFILE)

ICONLINKS = play_off.png play_on.png fastfwd_off.png fastfwd_on.png \
	fastback_off.png fastback_on.png back_off.png back_on.png \
	stepfwd_off.png stepfwd_on.png stepback_off.png stepback_on.png \
	stop_off.png stop_on.png aboutpcp.png aboutpmtime.png \
	whatsthis.png edit-clear.png internet-web-browser.png pmtime.png \
	$(ICNFILE) $(ICOFILE)

default: iconlinks wrappers
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
wrappers: $(WRAPPER)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).IN
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@

install: default
ifneq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
else
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
