#!gmake
#
# Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2012,2014,2017,2018 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives
SUBDIRS = multi multi_v3 multi-xz multi-xz-1k multi-corrupted \
	  multi_mixed

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
	  pcp-atop-boot.0.xz pcp-atop-boot.meta pcp-atop-boot.index \
	  pcp-atop-nvidia.0.xz pcp-atop-nvidia.meta.xz pcp-atop-nvidia.index \
	  pcp-atop-threads.0.xz pcp-atop-threads.meta.xz pcp-atop-threads.index \
	  pcp-dstat.0.xz pcp-dstat.meta pcp-dstat.index \
	  pcp-remotes.0.xz pcp-remotes.meta.xz pcp-remotes.index.xz \
	  pcp-free-tera.0.xz pcp-free-tera.meta.xz pcp-free-tera.index \
	  pcp-hotatop.0.xz pcp-hotatop.meta pcp-hotatop.index \
	  pcp-zeroconf.0.xz pcp-zeroconf.meta pcp-zeroconf.index \
	  value-test.0.xz value-test.meta value-test.index \
	  coverage.0.xz coverage.meta coverage.index \
	  bozo-20170904.0.xz bozo-20170904.meta bozo-20170904.index \
	  pcp-buddyinfo.0.xz pcp-buddyinfo.index.xz pcp-buddyinfo.meta.xz \
	  pcp-pidstat-process-states.0.xz pcp-pidstat-process-states.meta \
	  pcp-pidstat-process-states.index pcp-pidstat-process-args.0.xz \
	  pcp-pidstat-process-args.index pcp-pidstat-process-args.meta.xz \
	  pcp-pidstat-timezone.0.xz pcp-pidstat-timezone.meta.xz \
	  pcp-pidstat-timezone.index pcp-slabinfo.0.xz pcp-slabinfo.index.xz \
	  pcp-slabinfo.meta.xz pcp-zoneinfo.0.xz pcp-zoneinfo.index.xz \
	  pcp-zoneinfo.meta.xz zero_disk_activity.index  \
	  zero_disk_activity.0.xz zero_disk_activity.meta.xz \
	  dstat-disks.0.xz dstat-disks.meta.xz dstat-disks.index \
	  dstat-diskfarm.0.xz dstat-diskfarm.meta.xz dstat-diskfarm.index \
	  20180416.10.00.meta.xz 20180416.10.00.index 20180416.10.00.0.xz \
	  20180416.10.00.1.xz 20180416.10.00.2.xz 20180416.10.00.3.xz \
	  20190628.04.03.0.xz 20190628.04.03.1.xz 20190628.04.03.meta.xz \
	  20190628.04.03.index 20201109.0.xz 20201109.index 20201109.meta.xz \
	  20190628.06.31.0.xz 20190628.06.31.meta.xz 20190628.06.31.index \
	  pcp-ss.0.xz pcp-ss.meta.xz pcp-ss.index \
	  pcp-ps.0.xz pcp-ps.meta.xz pcp-ps.index all-ubuntu.22.04.index \
	  pcp-meminfo.0.xz pcp-meminfo.meta.xz pcp-meminfo.index \
	  pcp-netstat.0.xz pcp-netstat.meta.xz pcp-netstat.index \
	  pcp-mpstat.0.xz pcp-mpstat.meta.xz pcp-mpstat.index.xz \
	  pcp-rocestat.0.xz pcp-rocestat.meta.xz pcp-rocestat.index.xz \
	  all-ubuntu.22.04.0.xz all-ubuntu.22.04.meta.xz all-ubuntu.22.04.index \
	  bind2-9.18.0.xz bind2-9.18.meta.xz bind2-9.18.index \
	  bind2-9.18+.0.xz bind2-9.18+.meta.xz bind2-9.18+.index \
	  gfs.0.xz gfs.meta gfs.index \
	  amdgpu.0.xz amdgpu.meta.xz amdgpu.index \
	  nvidiagpu.0.xz nvidiagpu.meta.xz nvidiagpu.index


SCRIPTS = mk.diff mk.gap mk.mysql mk.schizo mk.foo+ \
	  mk.dynmetric mk.instant mk.dyninsts \
	  mk.mirage mk.mirage-3 mk.proc mk.free mk.ps\
	  mk.numa mk.rewrite mk.eventrec mk.interpmark mk.multihost \
	  mk.sample_expr mk.tzchange mk.arch-all mk.amdgpu mk.nvidiagpu \
	  mk.atop mk.hotatop mk.atop-boot mk.atop-nvidia mk.atop-threads \
	  mk.log-derived mk.vmstat mk.rep mk.procsched mk.netstat \
	  mk.ipcs mk.value-test mk.rank-pred mk.cputime mk.meminfo \
	  mk.sample-labels mk.pmproxy mk.mmv.help mk.slabinfo mk.zoneinfo \
	  mk.atop-sar mk.pmstat mk.rocestat mk.mpstat

CONFIGS = config.verify config.shping YWhcCi.toium.config \
	  kenj-pc-2.config reduce.config uwrap.config wrap.config \
	  coverage.config

OTHERS = README ds390-access.gz collectl-qa bigace.pmns \
	 crontab-sar-report.tar.xz cputime.mpstat cputime.sar \
	 farm.tar.xz atop-sar.tar.xz

LDIRT = pmlogger.log
LDIRDIRT = tmp
MANDIRT =

default default_pcp setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f $(SRCARCH) $(SRCNOTI) $(SRCPART) $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCMETA) $(SRCOTHER) $(TESTDIR)
	$(INSTALL) -m 644 -f $(SRCIDX) $(DEFLATE) $(TESTDIR)
	$(INSTALL) -m 644 -f $(OTHERS) $(CONFIGS) $(FOLIOS) $(TESTDIR)
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

# variant of the mpstat2 archive with log-once summary metrics
# 
pcp-mpstat3.0: pcp-mpstat2.0
	rm -rf tmp
	mkdir tmp
	pminfo -a pcp-mpstat2 \
		| sed \
			-e '/^kernel.uname./d' \
			-e '/^hinv.ncpu/d' \
		> tmp/config-body
	pminfo -a pcp-mpstat2 \
		| grep -E '^kernel.uname.|^hinv.ncpu' \
		> tmp/config-head
	pmlogextract -c tmp/config-head -s 1 pcp-mpstat2 pcp-mpstat3-head
	pmlogextract -c tmp/config-body pcp-mpstat2 pcp-mpstat3-body
	pmlogextract pcp-mpstat3-head pcp-mpstat3-body pcp-mpstat3
	mv pcp-mpstat3.0 tmp
	../src/stripmark tmp/pcp-mpstat3.0 pcp-mpstat3.0
	rm -rf tmp pcp-mpstat3-head.* pcp-mpstat3-body.*

# sample-proc (same as the one in ../tmparch but this one is not
# subject to platform jitters
#
sample-proc_v3.0 sammpe-proc_v2.0:
	../tmparch/make.sample-proc
