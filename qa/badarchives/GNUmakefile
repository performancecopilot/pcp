#!gmake
#
# Copyright (c) 2015-2017 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012,2014,2017 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/badarchives
SUBDIRS =

# volumes other than .0 or with compressed archive volumes
SRCPART = 20230506.20.02-01.0.xz 20230506.20.02-01.1.xz \
	  20230506.20.02-01.index 20230506.20.02-01.meta
# archives with no .index
SRCNOTI	= 
# other .meta files
SRCOTHER = bozo.client_v2.meta.bad.1 bozo.client_v2.meta.bad.2 \
	$(patsubst %.0,%.meta,$(SRCNOTI))

# now all the rest ...
SRCFULL = $(shell echo *.[0-9])
SRCARCH = $(filter-out $(SRCNOTI),$(SRCFULL))
SRCMETA = $(patsubst %.0,%.meta,$(SRCARCH))
SRCIDX = $(patsubst %.0,%.index,$(SRCARCH))

SCRIPTS = mk.badlen mk.badti mk.badlabel mk.badlog

LDIRT = pmlogger.log
MANDIRT =  # clear this - *.1.xz is an archive volume
LDIRDIRT = tmp

default default_pcp setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f $(SRCARCH) $(SRCNOTI) $(SRCPART) $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCMETA) $(SRCOTHER) $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 -f README $(TESTDIR)/README
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

# recipes for remaking some of the archives here
#

