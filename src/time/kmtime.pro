TEMPLATE	= app
LANGUAGE	= C++

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
	seealsodialog.ui \
	showboundsdialog.ui

IMAGES	= ../../images/aboutpcp.png \
	../../images/aboutqt.png \
	../../images/aboutkmtime.png \
	../../images/play_off.png \
	../../images/play_on.png \
	../../images/fastfwd_off.png \
	../../images/fastfwd_on.png \
	../../images/fastback_off.png \
	../../images/fastback_on.png \
	../../images/back_off.png \
	../../images/back_on.png \
	../../images/stepfwd_off.png \
	../../images/stepfwd_on.png \
	../../images/stepback_off.png \
	../../images/stepback_on.png \
	../../images/stop_off.png \
	../../images/stop_on.png \
	../../images/whatsthis.png \
	../../images/kmtime.png \
	../../images/edit-clear.png \
	../../images/internet-web-browser.png

#CONFIG	+= qt warn_on release
CONFIG	+= qt warn_on debug

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

