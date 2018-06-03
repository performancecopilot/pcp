#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/java
QAFILES = $(shell echo *.class *.java)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(QAFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

# rebuild the committed .class files for USDT testing
rebuild:
	javac USDTJVMTest.java
