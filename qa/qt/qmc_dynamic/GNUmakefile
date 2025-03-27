TOPDIR = ../../..

COMMAND = qmc_dynamic
PROJECT = $(COMMAND).pro
SOURCES = $(COMMAND).cpp

include $(TOPDIR)/src/include/builddefs

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
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f $(PROJECT) $(SOURCES) $(TESTDIR)
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 755 -f $(BINARY) $(TESTDIR)/$(COMMAND)
endif

include $(BUILDRULES)
