#
# Copyright (c) 2012-2015 Red Hat.
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/bigun

CFILES = bigun.c
LIBTARGET = bigun.$(DSOSUFFIX)
TARGETS = $(LIBTARGET)
MYFILES = domain.h help pmns root
MYSCRIPTS = Install Remove
LSRCFILES = $(MYFILES) $(MYSCRIPTS) GNUmakefile.install
LDIRT = help.pag help.dir

LLDFLAGS = $(PCP_LIBS)
LLDLIBS = $(PCP_PMDALIB)

default default_pcp setup: $(TARGETS)

$(LIBTARGET):	bigun.o

$(OBJECTS): domain.h

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
