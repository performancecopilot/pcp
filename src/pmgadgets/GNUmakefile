TOPDIR = ../..
COMMAND = pmgadgets
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
XMLFILE = $(COMMAND).info
HEADERS = pmgadgets.h tokens.h
SOURCES = $(HEADERS:.h=.cpp) main.cpp parse.cpp
CONFFILES = $(PROJECT)
LDIRT = $(COMMAND) $(ICNFILE) $(WRAPPER) $(SCRIPTS) $(XMLFILE) *.yy.c images

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me:: images wrappers
	+$(QTMAKE)
	$(LNMAKE)

build-me:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

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

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifneq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
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
