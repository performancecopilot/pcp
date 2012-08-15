TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COMMAND = qmc_indom
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp
TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/src/$(COMMAND)

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) *.o Makefile

default:
	$(QTMAKE)
	$(LNMAKE)

install: default
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(BINARY) $(TESTDIR)/$(COMMAND)

include $(BUILDRULES)
