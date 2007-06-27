TEMPLATE	= app
LANGUAGE	= C++

#CONFIG	+= qt warn_on release
CONFIG	+= qt warn_on debug

LIBS	+= -lpcp_pmc -lpcp -lqwt -lqassistantclient

INCLUDEPATH	+= /usr/include/qwt ../include

HEADERS	+= main.h \
	chart.h \
	namespace.h \
	qcolorpicker.h \
	source.h \
	tab.h \
	timeaxis.h \
	timecontrol.h \
	view.h

SOURCES	+= main.cpp \
	chart.cpp \
	namespace.cpp \
	qcolorpicker.cpp \
	source.cpp \
	tab.cpp \
	timeaxis.cpp \
	timecontrol.cpp \
	view.cpp

FORMS	= kmchart.ui \
	chartdialog.ui \
	tabdialog.ui \
	hostdialog.ui \
	aboutdialog.ui \
	seealsodialog.ui \
	settingsdialog.ui \
	infodialog.ui \
	recorddialog.ui

IMAGES	= ../../images/aboutpcp.png \
	../../images/aboutqt.png \
	../../images/aboutkmchart.png \
	../../images/document-new.png \
	../../images/document-open.png \
	../../images/document-print.png \
	../../images/media-record.png \
	../../images/document-save-as.png \
	../../images/kmtime.png \
	../../images/edit-clear.png \
	../../images/whatsthis.png \
	../../images/computer.png \
	../../images/process-stop.png \
	../../images/go-previous.png \
	../../images/system-search.png \
	../../images/help-browser.png \
	../../images/go-jump.png \
	../../images/kmchart.png \
	../../images/archive.png \
	../../images/filearchive.png \
	../../images/filegeneric.png \
	../../images/filehtml.png \
	../../images/fileimage.png \
	../../images/filepackage.png \
	../../images/filespreadsheet.png \
	../../images/fileview.png \
	../../images/filewordprocessor.png \
	../../images/tab-new.png \
	../../images/settings.png \
	../../images/folio.png \
	../../images/filefolio.png \
	../../images/view.png \
	../../images/logfile.png \
	../../images/document-properties.png \
	../../images/tab-edit.png \
	../../images/toolarchive.png \
	../../images/toolusers.png \
	../../images/toolview.png

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

