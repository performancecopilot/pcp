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
	   $(SOURCES) $(WRAPPER).in $(XMLFILE).in
LDIRT = $(COMMAND) $(WRAPPER) $(XMLFILE) images

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me: images wrappers
	+$(QTMAKE)
	$(LNMAKE)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
wrappers: $(WRAPPER) $(XMLFILE)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).in
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@
$(XMLFILE): $(XMLFILE).in
	$(SED) -e 's;PACKAGE_VERSION;$(PACKAGE_VERSION);g' < $< > $@

install: default
ifneq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
else
	$(INSTALL) -m 755 $(WRAPPER) $(PCP_BIN_DIR)/$(COMMAND)
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
else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

images: $(ICNFILE)
	$(LN_S) $(TOPDIR)/images images

$(ICNFILE):
	$(LN_S) $(TOPDIR)/images/$(ICNFILE) $(ICNFILE)
