TOPDIR = ../..
LIBRARY = libapp
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)
QRCFILE = $(shell echo *.qrc)
UIFILES = $(shell echo *.ui)
CONFFILES = $(PROJECT)

ICONLINKS = aboutpcp.png aboutqt.png aboutpmchart.png pmchart.png pmtime.png \
	    document-close.png document-new.png document-open.png \
	    document-print.png document-export.png document-properties.png \
	    document-save.png document-save-as.png emblem-system.png \
	    edit-clear.png settings.png camera-video-close.png \
	    archive.png computer.png camera-video.png system-search.png \
	    process-stop.png go-jump.png go-previous.png \
	    view-close.png view-new.png view-edit.png view-rotate.png \
	    help-contents.png help-browser.png whatsthis.png \
	    filearchive.png filefolder.png filegeneric.png filehtml.png \
	    fileimage.png filepackage.png filespreadsheet.png fileview.png \
	    filewordprocessor.png filefolio.png folio.png view.png logfile.png \
	    toolarchive.png toolusers.png toolview.png \
	    toolbackward.png toolforward.png \
	    play_live.png stop_live.png play_record.png stop_record.png \
	    play_archive.png stop_archive.png back_archive.png \
	    stepfwd_archive.png stepback_archive.png fastfwd_archive.png \
	    fastback_archive.png \

LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS) $(UIFILES) $(QRCFILE) \
	    $(ICONFILES)

LDIRT += $(ICONLINKS) version.h

default: version.h iconlinks $(PROJECT)
	$(QTMAKE)

version.h:
	rm -f version.h
	echo '#define PCP_VERSION "$(PCP_VERSION)"' >version.h

include $(BUILDRULES)

install: default
