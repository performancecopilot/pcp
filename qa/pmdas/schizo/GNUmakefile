#
# Copyright (c) 2012-2015 Red Hat.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM = schizo

TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/$(IAM)

CFILES = $(IAM)-A.c $(IAM)-B.c
TARGETS = pmda$(IAM)-A pmda$(IAM)-B
MYFILES = domain.h help-A help-B pmns-A pmns-B root-A root-B
MYSCRIPTS = Install Remove
LSRCFILES = $(MYSCRIPTS) $(MYFILES) GNUmakefile.install
LDIRT = help help.pag help.dir root pmns pmda$(IAM) pmda$(IAM)-A pmda$(IAM)-B

LLDFLAGS = $(PCP_LIBS)
LLDLIBS = $(PCP_PMDALIB)

default default_pcp setup: $(TARGETS)

pmda$(IAM)-A:	$(IAM)-A.o
	$(CCF) -o $@ $(LDFLAGS) $< $(LDLIBS)

pmda$(IAM)-B:	$(IAM)-B.o
	$(CCF) -o $@ $(LDFLAGS) $< $(LDLIBS)

$(OBJECTS): domain.h

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
