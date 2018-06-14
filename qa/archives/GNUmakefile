#!gmake
#
# Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012,2014,2017 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives
SUBDIRS = multi multi-xz multi-xz-1k

# first the special and odd ones
#
# volumes other than .0
SRCPART = ok-mv-bigbin.[1-9] ok-mv-foo.[12]
# archives with no .index
SRCNOTI	= ok-noti-bigbin.0 ok-noti-interp.0
# other .meta files
SRCOTHER = $(patsubst %.0,%.meta,$(SRCNOTI))

# now all the rest ...
SRCFULL = $(shell echo *.[0-9])
SRCARCH = $(filter-out $(SRCNOTI),$(SRCFULL))
SRCMETA = $(patsubst %.0,%.meta,$(SRCARCH))
SRCIDX = $(patsubst %.0,%.index,$(SRCARCH))

FOLIOS = $(shell echo *.folio)

DEFLATE = small.0.xz small.meta small.index \
	  20130706.0.bz2 20130706.meta 20130706.index \
	  20180606.0.xz 20180606.meta.xz 20180606.index \
	  pcp-atop.0.xz pcp-atop.meta pcp-atop.index \
	  pcp-hotatop.0.xz pcp-hotatop.meta pcp-hotatop.index \
	  pcp-zeroconf.0.gz pcp-zeroconf.meta pcp-zeroconf.index \
	  value-test.0.xz value-test.meta value-test.index \
	  coverage.0.xz coverage.meta coverage.index \
	  bozo-20170904.0.xz bozo-20170904.meta bozo-20170904.index \
	  pcp-pidstat-process-states.0.xz pcp-pidstat-process-states.meta \
	  pcp-pidstat-process-states.index

SCRIPTS = mk.diff mk.gap mk.mysql mk.schizo mk.foo+ \
	  mk.dynmetric mk.instant mk.dyninsts \
	  mk.mirage mk.proc \
	  mk.numa mk.rewrite mk.eventrec mk.interpmark mk.multihost \
	  mk.sample_expr mk.tzchange mk.arch-all mk.atop mk.hotatop \
	  mk.log-derived mk.vmstat mk.rep mk.procsched \
	  mk.ipcs mk.value-test mk.rank-pred

CONFIGS = config.verify config.shping YWhcCi.toium.config \
	  kenj-pc-2.config reduce.config uwrap.config wrap.config \
	  coverage.config

OTHERS = README ds390-access.gz collectl-qa bigace.pmns \
	 crontab-sar-report.tar.xz

LDIRT = pmlogger.log
LDIRDIRT = tmp

default default_pcp setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(SRCARCH) $(SRCNOTI) $(SRCPART) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCMETA) $(SRCOTHER) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCIDX) $(DEFLATE) $(TESTDIR)
	$(INSTALL) -m 644 $(OTHERS) $(CONFIGS) $(FOLIOS) $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

# recipes for remaking some of the archives here

mirage.0:
	rm -f mirage.index mirage.meta mirage.0
	pmlogger -c config.mirage -s 20 mirage

mirage-1.0 mirage-2.0:
	./mk.mirage

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

# variant of the base mpstat archive without interrupts metrics
# 
pcp-mpstat2.0:
	rm -rf tmp
	mkdir tmp
	pminfo -a pcp-mpstat \
		| sed \
			-e '/^kernel.percpu.interrupts/d' \
			-e '/^kernel.percpu.softirqs/d' \
			-e '/^event/d' -e '/^pmcd/d' \
		> tmp/config
	pmlogextract -c tmp/config pcp-mpstat pcp-mpstat2
	rm -rf tmp

