TEMPLATE	= app
LANGUAGE	= C++

HEADERS	+= kmquery.h

SOURCES	+= kmquery.cpp main.cpp

IMAGES	= ../../images/dialog-error.png \
	../../images/dialog-information.png \
	../../images/dialog-question.png \
	../../images/dialog-warning.png \
	../../images/dialog-archive.png \
	../../images/dialog-host.png

#CONFIG	+= qt warn_on release
CONFIG	+= qt warn_on debug

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

