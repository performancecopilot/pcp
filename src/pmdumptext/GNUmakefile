TOPDIR = ../..
COMMAND = pmdumptext
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

QRCFILE = $(COMMAND).qrc
UIFILES = $(shell echo *.ui)
SOURCES = pmdumptext.cpp
LDIRT = $(COMMAND)

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me:
	+$(QTMAKE)
	$(LNMAKE)

install: default
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
	rm $(BINARY)
else
	$(INSTALL) -m 755 $(BINARY) $(PCP_BIN_DIR)/$(COMMAND)
endif
else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

pmdumptext.o:	$(TOPDIR)/src/include/pcp/libpcp.h
