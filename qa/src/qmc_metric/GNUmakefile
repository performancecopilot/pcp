TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COMMAND = qmc_metric
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) *.o Makefile

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
