#!gmake
#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR	= $(PCP_VAR_DIR)/testsuite/src

LSRCFILES = $(MYFILES) $(MYSCRIPTS) GNUmakefile.install GNUlocaldefs

ifeq ($(shell test -f ../localconfig && echo 1), 1)
include ../localconfig
else
# Warning: ../localconfig does not exist, assuming PCP 3.0
PCP_VER=3000
endif

include GNUlocaldefs

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUlocaldefs $(TESTDIR)/GNUlocaldefs
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
