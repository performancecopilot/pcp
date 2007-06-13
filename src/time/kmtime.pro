TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on debug

LIBS	+= -lpcp -lqwt -lqassistantclient

INCLUDEPATH	+= /usr/include/qwt ../include

HEADERS	+= timelord.h \
	timezone.h \
	main.h

SOURCES	+= main.cpp

FORMS	= kmtimelive.ui \
	kmtimearch.ui \
	console.ui \
	aboutdialog.ui \
	aboutpcpdialog.ui \
	showboundsdialog.ui

IMAGES	= ../../images/play_off.xpm \
	../../images/play_on.xpm \
	../../images/fastfwd_off.xpm \
	../../images/fastfwd_on.xpm \
	../../images/fastback_off.xpm \
	../../images/fastback_on.xpm \
	../../images/back_off.xpm \
	../../images/back_on.xpm \
	../../images/stepfwd_off.xpm \
	../../images/stepfwd_on.xpm \
	../../images/stepback_off.xpm \
	../../images/stepback_on.xpm \
	../../images/stop_off.xpm \
	../../images/stop_on.xpm \
	../../images/whatsthis.png \
	../../images/aboutpcp.png \
	../../images/aboutkmtime.png \
	../../images/kmtime.png \
	../../images/edit-clear.png \
	../../images/internet-web-browser.png \
	../../images/menupcp.png

#CONFIG	+= qt warn_on release
CONFIG	+= qt warn_on debug

DEPENDPATH  += /usr/include/qwt

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

