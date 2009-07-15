#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include /etc/pcp.conf
endif
PATH	= $(shell . /etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

#DBG    = -DPCP_DEBUG -DDEBUG -DMALLOC_AUDIT -g
DBG     = -DPCP_DEBUG -DDEBUG -g

DEBUG   = $(DBG)

HAVE_64	= $(shell test `uname -m | sed -e 's/.*64/64/g'` = "64" && echo 1 || echo 0)

SRCARCH	= mirage.0 moomba.client.0 babylon.percpu.0 omen.client.0 \
	bozo.client.0 late.0 ok-bigbin.0 ok-mv-bigbin.0 ok-noti-bigbin.0 \
	babylon.pmview.0 ok-foo.0 ok-mv-foo.0 bug.0 ace.0 951127.23.01.0 \
	bigace.0 960624.08.17.0 rattle.0 mark-bug.0 arch_a.0 arch_b.0 \
	err_v1.0 changeinst.0 19970807.09.54.0 19970807.09.59.0 \
	binning.0 wrap.0 573184.0 xbug1.0 pcpcmd.0 markmerge.0 \
	sample-secs.0 gmt-boring.0 kenj-pc-1.0 kenj-pc-2.0 \
	reduce-1.0 reduce-2.0 kenj-pc-diskstat.0 snort-disks.0 \
	vldb-disks.0 moomba.pmkstat.0 fcsw_indom.0 \
	uwrap.0 20041125.0 ac15.0 conn20070309.0 logsummary-bug.0 \
	mirage-1.0 mirage-2.0

MKARCH	= foo.0 mv-foo.0 noti-foo.0 bar.0 mv-bar.0 noti-bar.0 \
	bigbin.0 mv-bigbin.0 noti-bigbin.0 truncbin.0 null.0 \
	interp.0 mv-interp.0 noti-interp.0 noti-ok-foo.0 gap.0 gap2.0 \
	dodgey-some.0 dodgey-all.0 dodgey-mixed.0 reduce-gap.0

ARCH	= $(SRCARCH) $(MKARCH)

CFILES = pducheck.c disk_test.c exercise.c context_test.c chkoptfetch.c \
	xval.c xxx.c chkopenlog.c chkconnect.c t_fetch.c \
	hp-mib.c agenttimeout.c chkctx2.c pmcdgone.c chkhelp.c \
	scale.c chktrim.c store.c torture_api.c logcontrol.c xlog.c \
	hex2nbo.c chknumval.c xarch.c eofarch.c defctx.c chkacc1.c chkacc2.c \
	chkacc3.c storepast.c pmdashutdown.c exertz.c badpmcdpmid.c \
	permfetch.c archinst.c pmlcmacro.c whichtimezone.c eol.c \
	interp0.c interp1.c interp2.c interp3.c interp4.c \
	pcp_lite_crash.c compare.c mkfiles.c nameall.c nullinst.c \
	storepdu.c fetchpdu.c badloglabel.c interp_bug2.c interp_bug.c \
	xmktime.c descreqX2.c recon.c torture_indom.c \
	fetchrate.c stripmark.c broken_pmda.c pmnsinarchives.c \
	endian.c chk_memleak.c chk_metric_types.c mark-bug.c \
	pmnsunload.c parsemetricspec.c pdu-server.c parseinterval.c \
	pmprintf.c pmsocks_objstyle.c numberstr.c \
	read-bf.c write-bf.c slow_af.c indom.c tztest.c \
	multifetch.c pmconvscale.c torture-eol.c \
	crashpmcd.c dumb_pmda.c torture_cache.c wrap_int.c \
	matchInstanceName.c mmv_genstats.c torture_pmns.c \
	record.c record-setarg.c clientid.c

POSIXFILES = \
	ipc.c proc_test.c context_fd_leak.c arch_maxfd.c torture_trace.c \
	pmpost-exploit.c 779246.c 

TRACEFILES = \
	obs.c tstate.c tabort.c 

ifneq "$(TARGET_OS)" "mingw"
CFILES += $(POSIXFILES) $(TRACEFILES)
else
LSRCFILES += $(POSIXFILES) $(TRACEFILES)
endif

CFILES_TARGETS = $(CFILES)

TARGETS = \
	$(subst broken_pmda,,$(basename $(CFILES_TARGETS))) \
	chkctx2_lite template \
	fetchrate_lite pv \
	broken_pmda_2_0 broken_v2.dir broken_pmda_2_1 broken_pmda_2_2 \
	broken_pmda_2_3 broken_pmda_2_4 broken_pmda_2_5 

ifeq ($(HAVE_64), 1)
TARGETS	+= pv64
endif

LCFLAGS = -DHAVE_TRACE
LLDLIBS	= -lpcp

# libpcp_trace is ready
TRACELIB	= -lpcp_trace

# all archives, except the ones checked into the source tree,
# then add executables beyond $(TARGETS)
#
LDIRT	= *.log domain.h help.pag help.dir mv-foo.? mv-bar.? mv-bigbin.? \
	$(subst .c,.o,$(CFILES)) \
	chkctx2_lite.c mv-interp.? fetchrate_lite.c \
	broken_pmda_*.c real_broken_pmns broken_v?.dir broken_v?.pag \
	broken_pmda_? localconfig.h gmon.out

.ORDER:	torture_api \
	chkctx2 fetchrate

default: $(CFILES_TARGETS) $(TARGETS) $(ARCH) ipcs_clear

clobber: clean-exec clean-arch
	rm -rf $(LDIRT) $(OBJECTS)

$(TARGETS):

install: default

# remove just the binaries, not the archives
#
clean-exec:
	rm -f $(TARGETS)

# remave just the archives, not the binaries
#
clean-arch:
	rm -f $(MKARCH) $(subst 0,meta,$(MKARCH)) $(subst 0,index,$(MKARCH))

OBJECTS = $(shell echo $(CFILES) | sed -e 's/\.c/.o/g')

# make just the archives, not the binaries
#
arch:	$(ARCH)

# --- do NOT need libpcp
#

pause:	pause.c
	$(CCF) $(CDEFS) -o pause pause.c

errmap: errmap.c
	$(CCF) $(CDEFS) -o errmap errmap.c

cpuload:	cpuload.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ cpuload.c

load_o2000:	load_o2000.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ load_o2000.c

chain:	chain.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chain.c

syscall:	syscall.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ syscall.c

getpagesize:	getpagesize.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ getpagesize.c

rdwr:	rdwr.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ rdwr.c

spawn:	spawn.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ spawn.c

diowr:	diowr.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ diowr.c

endian:	endian.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ endian.c

pmlcmacro:	pmlcmacro.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ pmlcmacro.c

sys5_ipc:	sys5_ipc.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ sys5_ipc.c

stripmark: stripmark.c
	$(CCF) $(CDEFS) -o $@ stripmark.c $(LDLIBS)

# --- need libpcp
#

template:	template.c
	rm -f $@
	$(CCF) $(CDEFS) -I/usr/include/pcp -o $@ template.c $(LDLIBS)

exercise:	exercise.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ exercise.c  $(LDLIBS)

chkacc1:	chkacc1.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chkacc1.c $(LDLIBS)

chkacc2:	chkacc2.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chkacc2.c $(LDLIBS)

chkacc3:	chkacc3.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chkacc3.c $(LDLIBS)

ipc:	ipc.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ ipc.c  $(LDLIBS)

chk_memleak:	chk_memleak.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chk_memleak.c $(LDLIBS)

chknumval:	chknumval.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ chknumval.c  $(LDLIBS)

logcontrol:	logcontrol.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ logcontrol.c  $(LDLIBS)

numberstr: numberstr.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ numberstr.c $(LDLIBS)

pmcdgone:	pmcdgone.c localconfig.h
	rm -f $@
	$(CCF) $(CDEFS) -o $@ pmcdgone.c  $(LDLIBS)

slow_af:	slow_af.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ slow_af.c  $(LDLIBS)

# --- need libpcp but also PM_CONTEXT_LOCAL
#

fetchrate_lite:	fetchrate_lite.c
	rm -f $@
	$(CCF) $(CDEFS) -DLITE=1 -o $@ fetchrate_lite.c $(LDLIBS)

fetchrate_lite.c:	fetchrate.c
	rm -f $@
	ln -s fetchrate.c fetchrate_lite.c

chkctx2_lite:	chkctx2_lite.c
	rm -f $@
	$(CCF) $(CDEFS) -DLITE=1 -o $@ chkctx2_lite.c $(LDLIBS)

chkctx2_lite.c:	chkctx2.c
	rm -f $@
	ln -s chkctx2.c chkctx2_lite.c

hp-mib: hp-mib.c
	rm -f $@
	$(CCF) $(CDEFS) -DLITE=1 -o $@ hp-mib.c $(LDLIBS)

pcp_lite_crash: pcp_lite_crash.c
	rm -f $@
	$(CCF) $(CDEFS) -DLITE=1 -o $@ pcp_lite_crash.c $(LDLIBS)

store: store.c
	rm -f $@
	$(CCF) $(CDEFS) -DLITE=1 -o $@ store.c $(LDLIBS)

# --- need libpcp_pmda
#

domain.h:	$(PCP_VAR_DIR)/pmns/stdpmid
	rm -f $@
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid on `date`" >>domain.h
	echo " */" >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]BROKEN[ 	]/s/.*[ 	]/#define BROKEN /p' $(PCP_VAR_DIR)/pmns/stdpmid >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]DYNAMIC[ 	]/s/.*[ 	]/#define DYNAMIC /p' $(PCP_VAR_DIR)/pmns/stdpmid >>domain.h

pmdashutdown:	pmdashutdown.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ pmdashutdown.c -lpcp_pmda $(LDLIBS)

pmdacomatose: pmdacomatose.c domain.h
	$(CCF) $(CDEFS) -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_v2.dir:	broken_help real_broken_pmns
	rm -f broken_v2.pag broken_v2.dir
	newhelp -n real_broken_pmns -v 2 -o broken_v2 broken_help

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
	$(CCF) $(CDEFS) -DVERSION_2 -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_pmda_2_1: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_1 -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_pmda_2_2: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_2 -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_pmda_2_3: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_3 -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_pmda_2_4: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_4 -o $@ $@.c -lpcp_pmda $(LDLIBS)

broken_pmda_2_5: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CCF) $(CDEFS) -DVERSION_2 -DBUG_5 -o $@ $@.c -lpcp_pmda $(LDLIBS)

dumb_pmda: dumb_pmda.c
	$(CCF) $(LCDEFS) $(LCOPTS) -o $@ $@.c -lpcp_pmda $(LDLIBS)

# --- need libpcp_gui
#

record:	record.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ $@.c -lpcp_gui $(LDLIBS)

record-setarg: record-setarg.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ $@.c -lpcp_gui $(LDLIBS)

# --- need libpcp_mmv
#
mmv_genstats:	mmv_genstats.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ $@.c -lpcp_mmv $(LDLIBS)

# --- need libpcp_trace 
#
__force_trace_dev:

trace_dev.h: __force_trace_dev
	@if [ -f ../../src/include/trace_dev.h -a -f trace_dev.h ]; \
	 then grep -v '\$$[I]d:' <../../src/include/trace_dev.h | grep -v '^[ 	]*\* ' >/tmp/$$$$.src; \
	      grep -v '\$$[I]d:' <trace_dev.h | grep -v '^[   ]*\* ' >/tmp/$$$$.qa; \
	      if diff /tmp/$$$$.src /tmp/$$$$.qa >/dev/null 2>&1; \
	      then rm -f /tmp/$$$$.*; \
	      else echo "Error: trace_dev.h and ../../src/include/trace_dev.h are different!"; \
		    rm -f /tmp/$$$$.*; \
		    exit 1; \
	      fi; \
	  fi

pducheck.o pdu-server.o : trace_dev.h

pducheck:	pducheck.o 
	rm -f $@
	$(CCF) $(CDEFS) -o $@ pducheck.o  $(TRACELIB) -lpcp_pmda $(LDLIBS)

pdu-server:	pdu-server.o
	rm -f $@
	$(CCF) $(CDEFS) -o $@ pdu-server.o  $(TRACELIB) $(LIBSOL) $(LDLIBS)

torture_cache:	torture_cache.o 
	rm -f $@
	$(CCF) $(CDEFS) -o $@ torture_cache.o -lpcp_pmda $(LDLIBS)

obs:	obs.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ obs.c $(TRACELIB) 

torture_trace:	torture_trace.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ torture_trace.c $(TRACELIB) 

tstate:	tstate.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ tstate.c $(TRACELIB) 

tabort:	tabort.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ tabort.c $(TRACELIB) 

# --- binary format dependencies
#

ifeq ($(HAVE_64), 1)
pv64: pv.c
	$(CCF) $(CDEFS) -o $@ pv.c $(LDLIBS)
endif

# -- archives
#

foo.0:	config.foo
	rm -f foo.index foo.meta foo.0
	pmlogger -c config.foo -s 8 foo

mv-foo.0:	config.foo
	rm -f mv-foo.index mv-foo.meta mv-foo.?
	pmlogger -c config.foo -s 8 -v 3 mv-foo

noti-foo.0:	foo.0
	cp foo.0 noti-foo.0
	cp foo.meta noti-foo.meta

noti-ok-foo.0:	ok-foo.0
	cp ok-foo.0 noti-ok-foo.0
	cp ok-foo.meta noti-ok-foo.meta

bar.0:	config.bar
	rm -f bar.index bar.meta bar.0
	pmlogger -c config.bar -s 70 bar

mv-bar.0:	config.bar
	rm -f mv-bar.index mv-bar.meta mv-bar.?
	pmlogger -c config.bar -s 70 -v 20 mv-bar

noti-bar.0:	bar.0
	cp bar.0 noti-bar.0
	cp bar.meta noti-bar.meta

mirage.0:
	rm -f mirage.index mirage.meta mirage.0
	pmlogger -c config.mirage -s 20 mirage

dodgey-some.0:	make.dodgey config.dodgey-some
	./make.dodgey some 50

dodgey-all.0:	make.dodgey config.dodgey-all
	./make.dodgey all 50

dodgey-mixed.0:	make.dodgey config.dodgey-mixed
	./make.dodgey mixed 150

bigbin.0:	config.bigbin
	rm -f bigbin.index bigbin.meta bigbin.0
	sync
	sleep 3
	pmlogger -c config.bigbin -s 1000 bigbin

mv-bigbin.0:	config.bigbin
	rm -f mv-bigbin.index mv-bigbin.meta mv-bigbin.?
	sync
	sleep 3
	pmlogger -c config.bigbin -s 1000 -v 101 mv-bigbin

noti-bigbin.0:	bigbin.0
	cp bigbin.0 noti-bigbin.0
	cp bigbin.meta noti-bigbin.meta

interp.0:	config.interp
	rm -f interp.index interp.meta interp.0
	pmlogger -c config.interp -s 30 interp

mv-interp.0:	config.interp
	rm -f mv-interp.index mv-interp.meta mv-interp.?
	pmlogger -c config.interp -s 30 -v 10 mv-interp

noti-interp.0:	interp.0
	cp interp.0 noti-interp.0
	cp interp.meta noti-interp.meta

truncbin.0:	bigbin.0
	rm -f truncbin.index truncbin.meta truncbin.0
	ln bigbin.meta truncbin.meta
	ln bigbin.index truncbin.index
	dd ibs=1024 count=200 if=bigbin.0 of=truncbin.0

# just the label record
null.0:	foo.0
	rm -f null.index null.meta null.0
	ln foo.meta null.meta
	ln foo.index null.index
	dd ibs=132 count=1 if=foo.0 of=null.0

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

# gap: 10 sec data, 5 sec gap, 10 sec data
# gap2: 10 sec data, 5 sec gap, 10 sec data, 5 sec gap, 10 sec data
# sample.bin instances change across gaps
#
gap.0 gap2.0:
	rm -rf tmp
	mkdir tmp
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-200","bin-400"] }' >tmp/A.config
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-300","bin-400"] }' >tmp/B.config
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-400"] }' >tmp/C.config
	pmlogger -s 10 -c tmp/A.config tmp/A
	sleep 5
	pmlogger -s 10 -c tmp/B.config tmp/B
	sleep 5
	pmlogger -s 10 -c tmp/C.config tmp/C
	rm -f gap.index gap.meta gap.0
	pmlogextract tmp/A tmp/B gap
	rm -f gap2.index gap2.meta gap2.0
	pmlogextract tmp/A tmp/B tmp/C gap2
	rm -rf tmp

reduce-gap.0:	reduce-1.0 reduce-2.0
	rm -f reduce-gap.*
	pmlogextract reduce-1 reduce-2 reduce-gap

torture_indom: torture_indom.c
	rm -f $@
	$(CCF) $(CDEFS) -o $@ torture_indom.c $(LDLIBS)

# --- miscellaneous
#
getpid.so:	getpid.o
	$(LD) -shared -o getpid.so -nostdlib getpid.o

arch_maxfd.o:	localconfig.h
scale.o:	localconfig.h

localconfig.h:
	cd ..; ./mk.localconfig && cp localconfig.h src-oss

bozo:
	@echo TARGETS=$(TARGETS)
