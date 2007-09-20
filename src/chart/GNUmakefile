TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmchart
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h chartdialog.h hostdialog.h infodialog.h \
	  kmchart.h recorddialog.h seealsodialog.h settingsdialog.h \
	  tabdialog.h chart.h main.h namespace.h qcolorpicker.h \
	  source.h tab.h timeaxis.h timecontrol.h
SOURCES = $(HEADERS:.h=.cpp) view.cpp

LSRCFILES = $(PROJECT) $(QRCFILE) $(UIFILES) $(HEADERS) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) $(ICONLINKS)

LCFLAGS = -I$(TOPDIR)/libqmc -I$(TOPDIR)/libqwt

ifeq ($(PKG_PLATFORM),darwin)
BINARY =  build/Default/$(COMMAND).app/Contents/MacOS/$(COMMAND)
LLDFLAGS = -L$(TOPDIR)/libqmc -L$(TOPDIR)/libqwt
else
BINARY =  $(COMMAND)
LLDFLAGS = -L$(TOPDIR)/libqmc/build/Default -L$(TOPDIR)/libqwt/build/Default
endif

ICONLINKS = aboutpcp.png aboutqt.png aboutkmchart.png camera-video.png \
	    document-new.png document-open.png document-print.png \
	    document-export.png kmtime.png edit-clear.png filefolder.png \
	    whatsthis.png computer.png process-stop.png go-previous.png \
	    system-search.png help-browser.png go-jump.png kmchart.png \
	    archive.png filearchive.png filegeneric.png filehtml.png \
	    fileimage.png filepackage.png filespreadsheet.png fileview.png \
	    filewordprocessor.png tab-new.png settings.png folio.png \
	    filefolio.png view.png logfile.png document-properties.png \
	    tab-edit.png toolarchive.png toolusers.png toolview.png \
	    toolbackward.png toolforward.png tooliconview.png toollistview.png \
	    play_live.png stop_live.png play_record.png stop_record.png \
	    play_archive.png stop_archive.png back_archive.png \
	    stepfwd_archive.png stepback_archive.png fastfwd_archive.png \
	    fastback_archive.png

SUBDIRS = views pmafm

default: iconlinks
	$(SUBDIRS_MAKERULE)
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
