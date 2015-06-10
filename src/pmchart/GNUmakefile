TOPDIR = ../..
COMMAND = pmchart
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
RCFILE  = $(COMMAND).rc
ICOFILE = $(COMMAND).ico
ICNFILE = $(COMMAND).icns
XMLFILE = $(COMMAND).info
DESKTOP = $(COMMAND).desktop
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h chartdialog.h exportdialog.h hostdialog.h \
	  infodialog.h pmchart.h openviewdialog.h saveviewdialog.h \
	  recorddialog.h seealsodialog.h searchdialog.h settingsdialog.h \
	  samplesdialog.h tabdialog.h tab.h tabwidget.h \
	  chart.h console.h main.h namespace.h \
	  colorbutton.h colorscheme.h qcolorpicker.h \
	  statusbar.h timeaxis.h timecontrol.h \
	  groupcontrol.h gadget.h sampling.h tracing.h
SOURCES = $(HEADERS:.h=.cpp) view.cpp
LSRCFILES = $(QRCFILE) $(RCFILE) $(UIFILES) $(HEADERS) $(SOURCES) \
	  $(PROJECT) $(DESKTOP) $(WRAPPER).in $(XMLFILE).in
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER) $(XMLFILE) images

SUBDIRS = views

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me:: images wrappers
	+$(QTMAKE)
	$(LNMAKE)

build-me:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

ifeq ($(WINDOW),mac)
PKG_MAC_DIR = /Applications/$(COMMAND).app/Contents
wrappers: $(WRAPPER) $(XMLFILE)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).in
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@
$(XMLFILE): $(XMLFILE).in
	$(SED) -e 's;PACKAGE_VERSION;$(PACKAGE_VERSION);g' < $< > $@

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
ifeq ($(WINDOW),win)
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
endif
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d $(PCP_DESKTOP_DIR)
	$(INSTALL) -m 644 $(DESKTOP) $(PCP_DESKTOP_DIR)/$(DESKTOP)
endif
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PCP_BIN_DIR)/$(COMMAND)
	$(call INSTALL_DIRECTORY_HIERARCHY,$(PKG_MAC_DIR),/Applications)
	$(INSTALL) -m 644 $(MACBUILD)/PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 644 $(XMLFILE) $(PKG_MAC_DIR)/Info.plist
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

images:	$(ICNFILE)
	$(LN_S) $(TOPDIR)/images images

$(ICNFILE):
	$(LN_S) $(TOPDIR)/images/$(ICNFILE) $(ICNFILE)
