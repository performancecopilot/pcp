TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COMMAND = qmc_format
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp
TESTDIR = $(PCP_VAR_DIR)/testsuite/qt/$(COMMAND)

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) *.o Makefile

default default_pcp setup:
ifeq "$(ENABLE_QT)" "true"
	$(QTMAKE)
	$(LNMAKE)
endif

install install_pcp: default
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(PROJECT) $(SOURCES) $(TESTDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 755 $(BINARY) $(TESTDIR)/$(COMMAND)
endif

include $(BUILDRULES)
