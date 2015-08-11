#!gmake
#
# Copyright (c) 2012,2014 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR	= $(PCP_VAR_DIR)/testsuite/src

LLDFLAGS = -L$(TOPDIR)/src/libpcp_trace/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_mmv/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_gui/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_import/$(LIBPCP_ABIDIR)

NVIDIACFLAGS = -I$(TOPDIR)/src/pmdas/nvidia
NVIDIAQALIB = libnvidia-ml.$(DSOSUFFIX)

LDIRT += localconfig.h

include GNUlocaldefs

localconfig.h: ../localconfig
	@awk -F= '{ print "#define",$$1,$$2 }' < ../localconfig > localconfig.h

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(PSCRIPTS) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TESTDIR)
	$(INSTALL) -m 755 $(NVIDIAQALIB) $(TESTDIR)/$(NVIDIAQALIB)
	$(INSTALL) -m 644 GNUlocaldefs $(TESTDIR)/GNUlocaldefs
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
