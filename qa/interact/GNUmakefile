TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/interact
TESTCASES = $(shell echo *-testcases)
LSRCFILES = $(TESTCASES)

default:

install: $(TESTCASES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(TESTCASES) $(TESTDIR)

include $(BUILDRULES)
