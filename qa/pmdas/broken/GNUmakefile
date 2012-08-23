#!gmake
#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2009-2010 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

STDPMID = $(TOPDIR)/src/pmns/stdpmid
TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/broken

CFILES = broken_pmda.c
CFILES_TARGETS = $(CFILES)

TARGETS = $(subst broken_pmda,,$(basename $(CFILES_TARGETS))) \
	broken_pmda_2_0 broken_v2.dir broken_pmda_2_1 broken_pmda_2_2 \
	broken_pmda_2_3 broken_pmda_2_4 broken_pmda_2_5 

LLDFLAGS = $(PCP_LIBS)
LLDLIBS	= -lpcp

# all archives, except the ones checked into the source tree,
# then add executables beyond $(TARGETS)
#
LDIRT	= *.log domain.h help.pag help.dir $(subst .c,.o,$(CFILES)) \
	broken_pmda_*.c real_broken_pmns broken_v?.dir broken_v?.pag \
	$(TARGETS)

default default_pcp setup: $(CFILES_TARGETS) $(TARGETS)

MYFILES = broken_help broken_pmns
MYSCRIPTS = broken_Install broken_Remove
LSRCFILES += $(MYFILES) $(MYSCRIPTS) GNUmakefile.install

include $(BUILDRULES)

$(TARGETS):

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

domain.h:	$(STDPMID)
	rm -f $@
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid on `date`" >>domain.h
	echo " */" >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]BROKEN[ 	]/s/.*[ 	]/#define BROKEN /p' $(STDPMID) >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]DYNAMIC[ 	]/s/.*[ 	]/#define DYNAMIC /p' $(STDPMID) >>domain.h

broken_v2.dir:	broken_help real_broken_pmns
	rm -f broken_v2.pag broken_v2.dir
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n real_broken_pmns -v 2 -o broken_v2 broken_help

real_broken_pmns:	domain.h broken_pmns
	cat domain.h >real_broken_pmns
	echo >>real_broken_pmns
	echo 'root {' >>real_broken_pmns
	echo '  broken' >>real_broken_pmns
	echo '}' >>real_broken_pmns
	echo >>real_broken_pmns
	cat broken_pmns >>real_broken_pmns

broken_pmda_2_0: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)

broken_pmda_2_1: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_1 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)

broken_pmda_2_2: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_2 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)

broken_pmda_2_3: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_3 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)

broken_pmda_2_4: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_4 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)

broken_pmda_2_5: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_5 -o $@ $@.c -lpcp_pmda $(LDLIBS) $(LDFLAGS)
