TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmchart
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc
UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.h chartdialog.h hostdialog.h infodialog.h \
	  kmchart.h recorddialog.h seealsodialog.h searchdialog.h \
	  settingsdialog.h tabdialog.h \
	  chart.h curve.h main.h namespace.h \
	  colorbutton.h colorscheme.h qcolorpicker.h \
	  tab.h tabwidget.h \
	  timeaxis.h timecontrol.h
SOURCES = $(HEADERS:.h=.cpp) view.cpp

LSRCFILES = $(PROJECT) $(QRCFILE) $(UIFILES) $(HEADERS) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) $(ICONLINKS)

LCFLAGS = -I$(TOPDIR)/libqmc -I$(TOPDIR)/libqwt

ifeq ($(PKG_PLATFORM),darwin)
LLDFLAGS = -L$(TOPDIR)/libqmc -L$(TOPDIR)/libqwt
else
LLDFLAGS = -L$(TOPDIR)/libqmc/build/Default -L$(TOPDIR)/libqwt/build/Default
endif

ICONLINKS = aboutpcp.png aboutqt.png aboutkmchart.png kmchart.png kmtime.png \
	    document-close.png document-new.png document-open.png \
	    document-print.png document-export.png document-properties.png \
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
ICONLINKS += kmchart.icns

SUBDIRS = views pmafm

default: iconlinks
	$(SUBDIRS_MAKERULE)
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
