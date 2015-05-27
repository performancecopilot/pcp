TOPDIR = ../../..
LIBRARY = libpcp_qed
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)

default: build-me

ifeq "$(ENABLE_QT)" "true"
build-me: $(PROJECT)
	+$(QTMAKE)
else
build-me:
endif

include $(BUILDRULES)

install: default

default_pcp:	default

install_pcp:	install
