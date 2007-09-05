TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmchart

UIFILES = aboutdialog.ui chartdialog.ui hostdialog.ui infodialog.ui \
	  kmchart.ui recorddialog.ui seealsodialog.ui settingsdialog.ui \
	  tabdialog.ui
HEADERS = aboutdialog.h chartdialog.h hostdialog.h infodialog.h \
	  kmchart.h recorddialog.h seealsodialog.h settingsdialog.h \
	  tabdialog.h chart.h main.h namespace.h qcolorpicker.h \
	  source.h tab.h timeaxis.h timecontrol.h
SOURCES = $(HEADERS:.h=.cpp) view.cpp

SUBDIRS = views pmafm
LSRCFILES = kmchart.pro kmchart.qrc $(UIFILES) $(SOURCES) $(HEADERS)
LDIRT = Makefile $(COMMAND) $(ICONLINKS)

ICONLINKS = aboutpcp.png aboutqt.png aboutkmchart.png \
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

default: iconlinks
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(COMMAND) $(PKG_BIN_DIR)
