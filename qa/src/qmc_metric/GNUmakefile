TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COMMAND = qmc_metric
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp
TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/src/$(COMMAND)

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) *.o Makefile

default:
	$(QTMAKE)
	$(LNMAKE)

install: default
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)

include $(BUILDRULES)
