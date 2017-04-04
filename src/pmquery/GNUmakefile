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
LSRCFILES = $(PROJECT) $(QRCFILE) $(SCRIPTS) $(HEADERS) $(SOURCES) \
	  $(WRAPPER).in pmconfirm.sh.in pmmessage.sh.in $(XMLFILE).in
LDIRT = $(COMMAND) $(ICNFILE) $(WRAPPER) $(SCRIPTS) $(XMLFILE) images

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me:: images wrappers
	+$(QTMAKE)
	$(LNMAKE)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Library/PCP/$(COMMAND).app/Contents
PKG_SUB_DIR = $(PKG_MAC_DIR)/MacOS
wrappers: $(WRAPPER) $(SCRIPTS) $(XMLFILE)
else
PKG_SUB_DIR = $(PCP_BIN_DIR)
wrappers: $(SCRIPTS)
endif

$(WRAPPER): $(WRAPPER).in
	@ $(SED) -e '/\# .*/b' -e 's;PCP_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmmessage.sh: pmmessage.sh.in
	@ $(SED) -e '/\# .*/b' -e 's;PCP_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
pmconfirm.sh: pmconfirm.sh.in
	@ $(SED) -e '/\# .*/b' -e 's;PCP_BIN_DIR;$(PKG_SUB_DIR);g' < $< > $@
$(XMLFILE): $(XMLFILE).in
	$(SED) -e 's;PACKAGE_VERSION;$(PACKAGE_VERSION);g' < $< > $@

install: default
ifneq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
endif
	$(INSTALL) -m 755 pmconfirm.sh $(PCP_BIN_DIR)/pmconfirm
	$(INSTALL) -m 755 pmmessage.sh $(PCP_BIN_DIR)/pmmessage
ifeq ($(WINDOW),mac)
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
