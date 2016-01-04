#!gmake
#
# Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012,2014 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives

# first the special and odd ones
#
# volumes other than .0
SRCPART = ok-mv-bigbin.[1-9] ok-mv-foo.[12]
# archives with no .index
SRCNOTI	= ok-noti-bigbin.0 ok-noti-interp.0
# other .meta files
SRCOTHER = bozo.client_v2.meta.bad.1 bozo.client_v2.meta.bad.2 \
	   $(patsubst %.0,%.meta,$(SRCNOTI))

# now all the rest ...
SRCFULL = $(shell echo *.[0-9]) 20130706.0.bz2
SRCARCH = $(filter-out $(SRCNOTI),$(SRCFULL))
SRCMETA = $(patsubst %.0,%.meta,$(SRCARCH)) 20130706.meta
SRCIDX = $(patsubst %.0,%.index,$(SRCARCH)) 20130706.index

FOLIOS = $(shell echo *.folio)

SCRIPTS = mk.diff.sh mk.gap.sh mk.mysql.sh mk.schizo.sh mk.foo+.sh \
	  mk.dynmetric.sh mk.instant.sh \
	  mkbadlen mkbadti mkbadlabel mkbadlog mkmirage mkproc \
	  mknuma mkrewrite mkeventrec mkinterpmark mkmultihost \
	  mksample_expr mktzchange mkarch-all mk.atop.sh \
	  mklog-derived mk.vmstat.sh

CONFIGS = config.verify config.shping YWhcCi.toium.config \
	  kenj-pc-2.config reduce.config uwrap.config wrap.config

OTHERS = ds390-access.gz collectl-qa small.0.xz small.meta small.index \
	bigace.pmns README

LDIRT = pmlogger.log

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(SRCARCH) $(SRCNOTI) $(SRCPART) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCMETA) $(SRCOTHER) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 $(OTHERS) $(CONFIGS) $(FOLIOS) $(TESTDIR)

include $(BUILDRULES)

# recipes for remaking some of the archives here

mirage.0:
	rm -f mirage.index mirage.meta mirage.0
	pmlogger -c config.mirage -s 20 mirage

mirage-1.0 mirage-2.0:
	./mkmirage

# 4 secs, 3 sec gap, 4 secs, 3 sec gap, 4 secs
# and indom changes ... checked in, this is the recipe that was
# used
#
changeinst.0:
	rm -rf tmp
	mkdir tmp
	echo 'log mandatory on once { hinv.ncpu }' >tmp/A.config
	echo 'log mandatory on 1sec {' >>tmp/A.config
	echo '  sample.seconds,hinv.ncpu,sample.bin["bin-100","bin-200","bin-400"]' >>tmp/A.config
	echo '  sample.drift' >>tmp/A.config
	echo '  irix.network.interface.total.packets["lo0","ec0"]' >>tmp/A.config
	echo '}' >>tmp/A.config
	echo 'log mandatory on once { sample.bin["bin-500"] }' >tmp/B.config
	echo 'log mandatory on 1sec {' >>tmp/B.config
	echo '  sample.seconds,sample.milliseconds,sample.bin["bin-100","bin-300","bin-400"]' >>tmp/B.config
	echo '}' >>tmp/B.config
	echo 'log mandatory on once { hinv.ncpu }' >tmp/C.config
	echo 'log mandatory on 1sec {' >>tmp/C.config
	echo '  sample.drift' >>tmp/C.config
	echo '  sample.seconds,sample.bin["bin-200","bin-300","bin-400","bin-500"]' >>tmp/C.config
	echo '  irix.network.interface.total.packets["ec2","lo0"]' >>tmp/C.config
	echo '}' >>tmp/C.config
	pmlogger -s 5 -c tmp/A.config tmp/A
	sleep 3
	pmlogger -s 5 -c tmp/B.config tmp/B
	sleep 3
	pmlogger -s 5 -c tmp/C.config tmp/C
	rm -f changeinst.index changeinst.meta changeinst.0
	pmlogextract tmp/A tmp/B tmp/C changeinst
	rm -rf tmp
