TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmchart
WRAPPER = $(COMMAND).sh
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
DESKTOP = $(COMMAND).desktop
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h chartdialog.h exportdialog.h hostdialog.h \
	  infodialog.h kmchart.h openviewdialog.h saveviewdialog.h \
	  recorddialog.h seealsodialog.h searchdialog.h settingsdialog.h \
	  tabdialog.h \
	  chart.h curve.h console.h main.h namespace.h \
	  colorbutton.h colorscheme.h qcolorpicker.h \
	  fileiconprovider.h statusbar.h tab.h tabwidget.h \
	  timeaxis.h timebutton.h timecontrol.h
SOURCES = $(HEADERS:.h=.cpp) view.cpp

LSRCFILES = $(PROJECT) $(QRCFILE) $(UIFILES) $(HEADERS) $(SOURCES) \
	  $(DESKTOP) $(WRAPPER).IN
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) $(ICONLINKS) $(WRAPPER)

ICONLINKS = aboutpcp.png aboutqt.png aboutkmchart.png kmchart.png kmtime.png \
	    $(ICNFILE) document-close.png document-new.png document-open.png \
	    document-print.png document-export.png document-properties.png \
	    document-save.png emblem-system.png \
	    edit-clear.png settings.png camera-video-close.png \
	    archive.png computer.png camera-video.png system-search.png \
	    process-stop.png go-jump.png go-previous.png \
	    tab-close.png tab-new.png tab-edit.png zoom-in.png zoom-out.png \
	    help-contents.png help-browser.png whatsthis.png \
	    filearchive.png filefolder.png filegeneric.png filehtml.png \
	    fileimage.png filepackage.png filespreadsheet.png fileview.png \
	    filewordprocessor.png filefolio.png folio.png view.png logfile.png \
	    toolarchive.png toolusers.png toolview.png \
	    toolbackward.png toolforward.png \
	    play_live.png stop_live.png play_record.png stop_record.png \
	    play_archive.png stop_archive.png back_archive.png \
	    stepfwd_archive.png stepback_archive.png fastfwd_archive.png \
	    fastback_archive.png

SUBDIRS = views pmafm

default: iconlinks wrappers
	$(SUBDIRS_MAKERULE)
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

ifeq ($(WINDOW),mac)
MACBUILD = $(COMMAND).app/Contents
PKG_MAC_DIR = /Applications/$(COMMAND).app/Contents
wrappers: $(WRAPPER)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).IN
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_DESKTOP_DIR)
	$(INSTALL) -m 644 $(DESKTOP) $(PKG_DESKTOP_DIR)
endif
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d /Applications
	$(INSTALL) -m 755 -d /Applications/$(COMMAND).app
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)
	$(INSTALL) -m 644 $(MACBUILD)/Info.plist $(PKG_MAC_DIR)/Info.plist
	$(INSTALL) -m 644 $(MACBUILD)/PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/MacOS
	$(INSTALL) -m 755 $(BINARY) $(PKG_MAC_DIR)/MacOS/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/Resources
	$(INSTALL) -m 644 $(MACBUILD)/Resources/$(ICNFILE) $(PKG_MAC_DIR)/Resources/$(ICNFILE)
endif
