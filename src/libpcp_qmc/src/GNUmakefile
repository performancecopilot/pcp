TOPDIR = ../../..
LIBRARY = libpcp_qmc
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me: $(PROJECT)
	+$(QTMAKE)
else
build-me:
endif

install: default

default_pcp:	default

install_pcp:	install
