TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COMMAND = qmc_indom
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND)

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
