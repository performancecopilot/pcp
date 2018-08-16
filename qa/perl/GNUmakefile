TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

PFILE = test.pl
CFILE = cvalue.c
LDLIBS = -lpcp
TESTDIR = $(PCP_VAR_DIR)/testsuite/perl
MAKEFILE = Makefile
LDIRT	= $(PFILE) $(CFILE) cvalue

default setup default_pcp: $(PFILE) $(CFILE)

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(PFILE) $(CFILE) $(MAKEFILE) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

# files from the perl/PMDA source directory
#
$(PFILE):
	cp $(TOPDIR)/src/perl/PMDA/$(PFILE) .
$(CFILE):
	cp $(TOPDIR)/src/perl/PMDA/$(CFILE) .

