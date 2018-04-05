TARGET		= pcp_qed
TEMPLATE	= lib
VERSION		= 1.0.0
CONFIG		+= qt staticlib warn_on
INCLUDEPATH	+= ../../include ../../libpcp_qmc/src
release:DESTDIR = build/debug
debug:DESTDIR   = build/release
QT		= core gui network printsupport svg widgets
QMAKE_CFLAGS	+= $$(PCP_CFLAGS) $$(CFLAGS)
QMAKE_CXXFLAGS	+= $$(PCP_CFLAGS) $$(CXXFLAGS)
QMAKE_LFLAGS	+= $$(LDFLAGS)

HEADERS	= qed.h \
	  qed_actionlist.h \
	  qed_app.h \
	  qed_bar.h \
	  qed_colorlist.h \
	  qed_colorpicker.h \
	  qed_console.h \
	  qed_fileiconprovider.h \
	  qed_gadget.h \
	  qed_groupcontrol.h \
	  qed_label.h \
	  qed_led.h \
	  qed_legend.h \
	  qed_line.h \
	  qed_recorddialog.h \
	  qed_statusbar.h \
	  qed_timebutton.h \
	  qed_timecontrol.h \
	  qed_viewcontrol.h \

SOURCES = \
	  qed_actionlist.cpp \
	  qed_app.cpp \
	  qed_bar.cpp \
	  qed_colorlist.cpp \
	  qed_colorpicker.cpp \
	  qed_console.cpp \
	  qed_fileiconprovider.cpp \
	  qed_gadget.cpp \
	  qed_groupcontrol.cpp \
	  qed_label.cpp \
	  qed_led.cpp \
	  qed_legend.cpp \
	  qed_line.cpp \
	  qed_recorddialog.cpp \
	  qed_statusbar.cpp \
 	  qed_timebutton.cpp \
	  qed_timecontrol.cpp \
	  qed_viewcontrol.cpp \

FORMS = \
	  qed_console.ui \
	  qed_recorddialog.ui \
