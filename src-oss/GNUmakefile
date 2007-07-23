#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# $Id: GNUmakefile,v 1.32 2007/02/26 06:06:46 kimbrr Exp $
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include /etc/pcp.conf
endif
PATH	= $(shell . /etc/pcp.env; echo $$PATH:/usr/local/bin/ptools)

ifneq ($(ROOT)$(TOOLROOT), )
root-snarf:
	@echo 'unsetenv ROOT TOOLROOT'
	@echo 'or unset ROOT TOOLROOT'
	@echo 'and try again, bozo ...'
	@false
endif

OBJSTYLE =

ifeq ($(PCP_PLATFORM), irix)
include /usr/include/make/commondefs
include versiondefs

LIBGEN = -lgen

OBJSTYLE = n32
ifeq ($(IRIX), IRIX5_3)
OBJSTYLE = o32
endif
ifeq ($(IRIX), IRIX6_2)
OBJSTYLE = o32
endif
ifeq ($(IRIX), IRIX6_3)
OBJSTYLE = o32
endif
ifeq ($(IRIX), IRIX6_4)
OBJSTYLE = o32
endif

ifeq ($(OBJSTYLE), o32)
LIBPCP2 = /usr/lib/libpcp.so.2
LIBPMDA2 = /usr/lib/libpcp_pmda.so.2
LIBTRACE = /usr/lib/libpcp_trace.so.1
LIBPMC = /usr/lib/libpcp_pmc.a
OPTS_32 = -o32 -mips1
else
LIBPCP2 = /usr/lib32/libpcp.so.2
LIBPMDA2 = /usr/lib32/libpcp_pmda.so.2
LIBTRACE = /usr/lib32/libpcp_trace.so.1
OPTS_32 = -n32 -mips3
endif

else
# not IRIX

# libpcp_pmc is included in the open source version, but does not
# get installed ... need the headers and the static library to live
# in this directory
#
INCPMC	= -I../../src/libpcp_pmc/src
LIBPMC = ../../src/libpcp_pmc/src/libpcp_pmc.so.1

ifeq ($(PCP_PLATFORM), solaris)
LIBSOL = -lsocket

else
# not Irix or Solaris
RPATH = -Wl,-rpath,/usr/share/pcp/compat
LIBPCP2 = -lpcp
LIBPMDA2 = -lpcp_pmda
LIBGEN =
endif
endif

HAVE_DYNAMIC	= $(shell grep DYNAMIC $(PCP_VAR_DIR)/pmns/stdpmid >/dev/null && echo 1; exit 0)

# now, libpcp_pmc is _in_ the open source distribution, this is no longer
# conditional
# HAVE_PMC	= $(shell [ -f $(LIBPMC) ] && echo 1; exit 0)
HAVE_PMC	= 1

LITE_LDLIBS	= -lpcp

#DBG    = -DPCP_DEBUG -DDEBUG -DMALLOC_AUDIT -g
DBG     = -DPCP_DEBUG -DDEBUG -g
#DBG    = -DPCP_DEBUG -DDEBUG

DEBUG   = $(DBG)

HAVE_64		= $(shell test `uname -m` = "ia64" && echo 1 || echo 0)

ifeq ($(PCP_PLATFORM), irix)
DEBUG	+= -D$(IRIX)=1
# when DEBUG includes -g, turn off optimization
ifneq ($(findstring $(DEBUG),-g), )
OPTIMIZER	= -O0
endif
ifneq ($(findstring $(IRIX_OBJECT),64), )
HAVE_64		= 1
endif
endif

SRCARCH	= mirage.0 moomba.client.0 babylon.percpu.0 omen.client.0 \
	bozo.client.0 late.0 ok-bigbin.0 ok-mv-bigbin.0 ok-noti-bigbin.0 \
	babylon.pmview.0 ok-foo.0 ok-mv-foo.0 bug.0 ace.0 951127.23.01.0 \
	bigace.0 960624.08.17.0 rattle.0 mark-bug.0 arch_a.0 arch_b.0 \
	err_v1.0 changeinst.0 19970807.09.54.0 19970807.09.59.0 \
	binning.0 wrap.0 573184.0 xbug1.0 pcpcmd.0 markmerge.0 \
	sample-secs.0 gmt-boring.0 kenj-pc-1.0 kenj-pc-2.0 \
	reduce-1.0 reduce-2.0 kenj-pc-diskstat.0 snort-disks.0 \
	vldb-disks.0 oview-short.0 moomba.pmkstat.0 fcsw_indom.0 \
	uwrap.0 20041125.0 ac15.0

MKARCH	= foo.0 mv-foo.0 noti-foo.0 bar.0 mv-bar.0 noti-bar.0 \
	bigbin.0 mv-bigbin.0 noti-bigbin.0 truncbin.0 null.0 \
	interp.0 mv-interp.0 noti-interp.0 noti-ok-foo.0 gap.0 gap2.0 \
	dodgey-some.0 dodgey-all.0 dodgey-mixed.0 reduce-gap.0

ARCH	= $(SRCARCH) $(MKARCH)

# Try and keep this initial CFILES block the same in Makefile
# and GNUmakefile for all common apps that are used in both IRIX
# and Linux

CFILES = pducheck.c disk_test.c exercise.c context_test.c chkoptfetch.c \
	ipc.c xval.c xxx.c chkopenlog.c chkconnect.c t_fetch.c \
	proc_test.c hp-mib.c agenttimeout.c chkctx2.c pmcdgone.c chkhelp.c \
	scale.c chktrim.c store.c torture_api.c logcontrol.c xlog.c \
	hex2nbo.c chknumval.c xarch.c eofarch.c defctx.c chkacc1.c chkacc2.c \
	chkacc3.c storepast.c pmdashutdown.c exertz.c badpmcdpmid.c \
	permfetch.c archinst.c pmlcmacro.c whichtimezone.c eol.c \
	interp0.c interp1.c interp2.c interp3.c interp4.c \
	pcp_lite_crash.c compare.c mkfiles.c nameall.c nullinst.c \
	storepdu.c fetchpdu.c badloglabel.c interp_bug2.c interp_bug.c \
	xmktime.c descreqX2.c context_fd_leak.c recon.c torture_indom.c \
	fetchrate.c stripmark.c obs.c broken_pmda.c pmnsinarchives.c \
	endian.c chk_memleak.c chk_metric_types.c arch_maxfd.c mark-bug.c \
	pmnsunload.c parsemetricspec.c pdu-server.c parseinterval.c \
	pmprintf.c pmsocks_objstyle.c torture_trace.c numberstr.c tstate.c \
	read-bf.c write-bf.c slow_af.c tabort.c indom.c 779246.c tztest.c \
	pmpost-exploit.c multifetch.c pmconvscale.c torture-eol.c \
	crashpmcd.c dumb_pmda.c torture_cache.c wrap_int.c

# TODO need re-writing for Linux?
OLD_CFILES	= pmda_sproc.c sys5_ipc.c diowr.c pmie-lic.c \
	   sysinfocounts.c load_o2000.c 

ifeq ($(HAVE_DYNAMIC), 1)
CFILES	+= dynamic_pmda.c
endif

CFILES_TARGETS = $(CFILES)

CXXFILES	=

ifeq ($(HAVE_PMC), 1)
CXXFILES	+= pmc_list.c++ pmc_context.c++ \
		pmc_format.c++ pmc_vector.c++ pmc_string.c++ \
		pmc_source.c++ pmc_desc.c++ pmc_indom.c++ pmc_context.c++ \
		pmc_metric.c++ pmc_group.c++ pmc_dynamic.c++ pmc_hosts.c++
endif

TARGETS = \
	$(subst broken_pmda,,$(basename $(CFILES_TARGETS))) \
	pminfo_lite chkctx2_lite pmclient template \
	fetchrate_lite pv pmval \
	broken_pmda_2_0 broken_v2.dir broken_pmda_2_1 broken_pmda_2_2 \
	broken_pmda_2_3 broken_pmda_2_4 broken_pmda_2_5 

ifeq ($(HAVE_64), 1)
TARGETS	+= pv64
endif

ifeq ($(HAVE_PMC), 1)
TARGETS		+= pmc_list pmc_context \
		pmc_format pmc_vector pmc_list pmc_string \
		pmc_source pmc_desc pmc_indom pmc_metric pmc_group \
		pmc_dynamic pmc_hosts
endif

HAVE_MONS_H := $(shell test -r /usr/include/pcp/pmapi_mon.h && echo "-DHAVE_SEPARATE_MONS_H")

COPTS	=
CDEFS	= $(DEBUG) $(HAVE_MONS_H)
CFLAGS	= $(CDEFS) $(COPTS) 
LDOPTS = 
LDLIBS	= -lpcp

LCXXOPTS = -I/usr/include/pcp -Wno-deprecated
LCXXDEFS = $(DEBUG) $(HAVE_MONS_H)
LCXXINCS =

ifeq ($(PCP_PLATFORM), irix)
COPTS	+= -fullwarn -MDupdate Makedepend
LCXXOPTS += -MDupdate Makedepend
ifeq ($(OBJSTYLE), o32)
LCXXOPTS += $(OPTS_32)
else
LCXXOPTS += -n32
endif
endif

# libpcp_trace is ready
COPTS		+= -DHAVE_TRACE
TRACELIB	= -lpcp_trace

ifeq ($(HAVE_64), 1)
COPTS += -fpic -fno-strict-aliasing
endif

# all archives, except the ones checked into the source tree,
# then add executables beyond $(TARGETS)
#
LDIRT	= *.log domain.h help.pag help.dir mv-foo.? mv-bar.? mv-bigbin.? \
	$(subst .c,.o,$(CFILES)) $(subst .c++,.o,$(CXXFILES)) \
	chkctx2_lite.c pmclient_src mv-interp.? fetchrate_lite.c \
	real_dynamic_pmns dynamic.dir dynamic.pag \
	broken_pmda_*.c real_broken_pmns broken_v?.dir broken_v?.pag \
	broken_pmda_? pv64.c localconfig.h gmon.out

.ORDER:	torture_api \
	chkctx2 fetchrate

LIB64PCP1 = /usr/lib64/libpcp.so.1
LIB64PCP2 = /usr/lib64/libpcp.so.2
LIBPCP1 = /usr/lib/libpcp.so.1
LIBPMDA1 = /usr/lib/libpcp_pmda.so.1
OPTS_V1 = -o32 -mips1

COPTS1 = -fullwarn -I/usr/include/pcp1.x -I/usr/include/pcp1.x/pcp -MDupdate Makedepend

default: $(CFILES_TARGETS) $(TARGETS) $(ARCH) ipcs_clear

clobber: clean-exec clean-arch
	rm -rf $(LDIRT) $(OBJECTS)

$(TARGETS):

ifeq ($(PCP_PLATFORM), irix)
include $(COMMONRULES)
endif

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
OBJECTS += $(shell echo $(CXXFILES) | sed -e 's/\.c++/.o/g')

# make just the archives, not the binaries
#
arch:	$(ARCH)

# --- do NOT need libpcp
#

pause:	pause.c
	$(CC) $(CDEFS) $(COPTS) -o pause pause.c

errmap: errmap.c
	$(CC) $(CDEFS) $(COPTS) -o errmap errmap.c

sproc_mem_share:	sproc_mem_share.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ sproc_mem_share.c -lpthread

cpuload:	cpuload.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ cpuload.c

load_o2000:	load_o2000.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ load_o2000.c

chain:	chain.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chain.c

syscall:	syscall.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ syscall.c

getpagesize:	getpagesize.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ getpagesize.c

rdwr:	rdwr.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ rdwr.c

spawn:	spawn.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ spawn.c

diowr:	diowr.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ diowr.c

endian:	endian.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ endian.c

pmlcmacro:	pmlcmacro.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ pmlcmacro.c

sys5_ipc:	sys5_ipc.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ sys5_ipc.c

stripmark: stripmark.c
	$(CC) $(CDEFS) $(COPTS) -o $@ stripmark.c

# --- need libpcp
#

template:	template.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -I/usr/include/pcp -o $@ template.c $(LDLIBS)

pmie-lic:	pmie-lic.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ pmie-lic.c  -llmsgi $(LDLIBS)

exercise:	exercise.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ exercise.c  $(LDLIBS)

pmdashutdown:	pmdashutdown.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ pmdashutdown.c $(LIBSOL) -lpcp_pmda $(LDLIBS)

chkacc1:	chkacc1.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chkacc1.c -lresolv  $(LDLIBS)

chkacc2:	chkacc2.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chkacc2.c  -lresolv $(LDLIBS)

chkacc3:	chkacc3.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chkacc3.c -lresolv -lnsl $(LDLIBS)

ipc:	ipc.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ ipc.c  $(LDLIBS)

auth_client:	auth_client.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ auth_client.c  $(LDLIBS)

fakebrand:	fakebrand.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ fakebrand.c  -llmsgi $(LDLIBS)

chk_memleak:	chk_memleak.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chk_memleak.c -lpcp $(LDLIBS)

chknumval:	chknumval.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ chknumval.c  $(LDLIBS)

logcontrol:	logcontrol.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ logcontrol.c  $(LDLIBS)

pmcdgone:	pmcdgone.c localconfig.h
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ pmcdgone.c  $(LDLIBS)

pmbrand_qa:	pmbrand.c
	rm -f $@
	$(CC) -DQA=1 $(CDEFS) $(COPTS) -o $@ pmbrand.c  -llmsgi $(LDLIBS)

slow_af:	slow_af.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ slow_af.c  $(LDLIBS)

pmval:	pmval.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ $@.c  $(LDLIBS) -lm

real_dynamic_pmns:	domain.h dynamic_pmns
	cat domain.h >real_dynamic_pmns
	echo >>real_dynamic_pmns
	echo 'root {' >>real_dynamic_pmns
	echo '  broken' >>real_dynamic_pmns
	echo '}' >>real_dynamic_pmns
	echo >>real_dynamic_pmns
	cat dynamic_pmns >>real_dynamic_pmns

dynamic.dir:	dynamic_help real_dynamic_pmns
	rm -f dynamic.pag dynamic.dir
	newhelp -n real_dynamic_pmns -v 2 -o dynamic dynamic_help

dynamic_pmda: dynamic_pmda.c domain.h real_dynamic_pmns
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

# --- need libpcp but also PM_CONTEXT_LOCAL
#

fetchrate_lite:	fetchrate_lite.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DLITE=1 $(COPTS) -o $@ fetchrate_lite.c $(LDOPTS) $(LITE_LDLIBS)

fetchrate_lite.c:	fetchrate.c
	rm -f $@
	ln -s fetchrate.c fetchrate_lite.c

chkctx2_lite:	chkctx2_lite.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DLITE=1 $(COPTS) -o $@ chkctx2_lite.c $(LDOPTS) $(LITE_LDLIBS)

chkctx2_lite.c:	chkctx2.c
	rm -f $@
	ln -s chkctx2.c chkctx2_lite.c

hp-mib: hp-mib.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DLITE=1 $(COPTS) -o $@ hp-mib.c $(LDOPTS) $(LITE_LDLIBS)

pcp_lite_crash: pcp_lite_crash.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DLITE=1 $(COPTS) -o $@ pcp_lite_crash.c $(LDOPTS) $(LITE_LDLIBS)

store: store.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DLITE=1 $(COPTS) -o $@ store.c $(LDOPTS) $(LITE_LDLIBS)

pminfo_lite:	pminfo.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) -DPM_USE_CONTEXT_LOCAL=1 -I/usr/include/pcp $(COPTS) -o $@ pminfo.c $(LDOPTS) $(LITE_LDLIBS)

# --- need libpcp_pmda
#

domain.h:	$(PCP_VAR_DIR)/pmns/stdpmid
	rm -f $@
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid on `date`" >>domain.h
	echo " */" >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]BROKEN[ 	]/s/.*[ 	]/#define BROKEN /p' $(PCP_VAR_DIR)/pmns/stdpmid >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]DYNAMIC[ 	]/s/.*[ 	]/#define DYNAMIC /p' $(PCP_VAR_DIR)/pmns/stdpmid >>domain.h

pmdacomatose: pmdacomatose.c domain.h
	$(CC) $(CDEFS) $(COPTS) -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_v2.dir:	broken_help real_broken_pmns
	rm -f broken.pag broken.dir
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
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_pmda_2_1: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -DBUG_1 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_pmda_2_2: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -DBUG_2 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_pmda_2_3: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -DBUG_3 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_pmda_2_4: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -DBUG_4 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

broken_pmda_2_5: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(CDEFS) $(COPTS) -DVERSION_2 -DBUG_5 -o $@ $@.c $(LDOPTS) -lpcp_pmda $(LDLIBS)

dumb_pmda: dumb_pmda.c
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

# --- special cases - hand-crafted libraries required
#

xpmval: xpmval.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ xpmval.c $(LDOPTS) -lm $(LDLIBS)

numberstr: numberstr.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ numberstr.c $(LDOPTS) $(LIBGEN) $(LDLIBS)

# --- need libpcp_trace 
#
__force_trace_dev:

trace_dev.h: __force_trace_dev
	@if [ -f ../../src/include/trace_dev.h -a -f trace_dev.h ]; \
	 then grep -v '\$$[I]d:' <../../src/include/trace_dev.h >/tmp/$$$$.src; \
	      grep -v '\$$[I]d:' <trace_dev.h >/tmp/$$$$.qa; \
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
	$(CC) $(CDEFS) $(COPTS) -o $@ pducheck.o  $(TRACELIB) -lpcp_pmda $(LDLIBS)

pdu-server:	pdu-server.o
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ pdu-server.o  $(TRACELIB) $(LIBSOL) $(LDLIBS)

torture_cache:	torture_cache.o 
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ torture_cache.o -lpcp_pmda $(LDLIBS)

obs:	obs.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ obs.c $(TRACELIB) 

torture_trace:	torture_trace.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ torture_trace.c $(TRACELIB) 

tstate:	tstate.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ tstate.c $(TRACELIB) 

tabort:	tabort.c
	rm -f $@
	$(CC) $(CDEFS) $(COPTS) -o $@ tabort.c $(TRACELIB) 

# --- $(IRIX_OBJECT) format dependencies
#

ifeq ($(HAVE_64), 1)
pv64: pv.c
	rm -f $@ $@.c
	ln -s pv.c $@.c
	$(CC) $(CDEFS) $(COPTS) -o $@ $@.c $(LDOPTS) -lpcp
else
pv64:
	rm -f $@
	ln -s pv pv64
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

pmclient:	pmclient_src pmclient_src/pmclient.c \
		pmclient_src/pmnsmap.spec pmclient_src/Makefile
	rm -f pmclient pmclient_src/pmclient
	cd pmclient_src ; $(MAKE) pmclient
	ln pmclient_src/pmclient pmclient

pmclient_src: $(PCP_DEMOS_DIR)/pmclient/pmclient.c
	- rm -rf pmclient_src
	mkdir pmclient_src
	cp $(PCP_DEMOS_DIR)/pmclient/* pmclient_src
	cd pmclient_src ; $(MAKE) clobber

pmclient_src/pmclient.c:	$(PCP_DEMOS_DIR)/pmclient/pmclient.c
	rm -f pmclient_src/pmclient.c
	cp $(PCP_DEMOS_DIR)/pmclient/pmclient.c pmclient_src

pmclient_src/pmnsmap.spec:	$(PCP_DEMOS_DIR)/pmclient/pmnsmap.spec
	rm -f pmclient_src/pmnsmap.spec
	cp $(PCP_DEMOS_DIR)/pmclient/pmnsmap.spec pmclient_src

pmclient_src/Makefile:	$(PCP_DEMOS_DIR)/pmclient/Makefile
	rm -f pmclient_src/Makefile
	cp $(PCP_DEMOS_DIR)/pmclient/Makefile pmclient_src

torture_indom: torture_indom.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(CDEFS) $(COPTS) -o $@ torture_indom.c $(LDOPTS) $(LDLIBS)

# --- libpcp_pmc
#
pmc_format.o	:	pmc_format.c++
	$(CXX) -c pmc_format.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_format	:	pmc_format.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_vector.o	:	pmc_vector.c++
	$(CXX) -c pmc_vector.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_vector	:	pmc_vector.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPCP2) $(LIBGEN)

pmc_list.o	:	pmc_list.c++
	$(CXX) -c pmc_list.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_list	:	pmc_list.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPCP2) $(LIBGEN)

pmc_string.o	:	pmc_string.c++
	$(CXX) -c pmc_string.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_string	:	pmc_string.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_source.o	:	pmc_source.c++
	$(CXX) -c pmc_source.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_source	:	pmc_source.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_desc.o	:	pmc_desc.c++
	$(CXX) -c pmc_desc.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_desc	:	pmc_desc.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_indom.o	:	pmc_indom.c++
	$(CXX) -c pmc_indom.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_indom	:	pmc_indom.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_context.o	:	pmc_context.c++
	$(CXX) -c pmc_context.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_context	:	pmc_context.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_metric.o	:	pmc_metric.c++
	$(CXX) -c pmc_metric.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_metric	:	pmc_metric.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_group.o	:	pmc_group.c++
	$(CXX) -c pmc_group.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_group	:	pmc_group.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_dynamic.o	:	pmc_dynamic.c++
	$(CXX) -c pmc_dynamic.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_dynamic	:	pmc_dynamic.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

pmc_hosts.o	:	pmc_hosts.c++
	$(CXX) -c pmc_hosts.c++ $(LCXXOPTS) $(LCXXDEFS) $(INCPMC)
pmc_hosts	:	pmc_hosts.o $(LIBPMC)
	$(CXX) -o $@ $@.o $(LCXXOPTS) $(LCXXDEFS) $(LIBPMC) $(LIBPCP2) $(LIBGEN)

# --- miscellaneous
#
getpid.so:	getpid.o
	$(LD) -shared -o getpid.so -nostdlib getpid.o

arch_maxfd.o:	localconfig.h
scale.o:	localconfig.h

localconfig.h:
	cd ..; ./mk.localconfig && cp localconfig.h src-oss

bozo:
	@echo LIBPMC=$(LIBPMC)
	@echo HAVE_PMC=$(HAVE_PMC)
	@echo TARGETS=$(TARGETS)
