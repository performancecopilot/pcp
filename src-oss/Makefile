#!smake -J1
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# $Id: Makefile,v 1.23 2004/04/13 08:59:30 kenmcd Exp $
#

include versiondefs
include /usr/include/make/commondefs

CSTYLE	= $(DEF_CSTYLE)
GLDOPTS	= $(DEF_GLDOPTS)

#if defined(ROOT) || defined(TOOLROOT)
.BEGIN:
	@echo 'unsetenv ROOT TOOLROOT'
	@echo 'or unset ROOT TOOLROOT'
	@echo 'and try again, bozo ...'
	@false
#endif

#if $(IRIX) == "IRIX6_2"
LIBPCP2 = /usr/lib/libpcp.so.2
LIBPMDA2 = /usr/lib/libpcp_pmda.so.2
LIBTRACE = /usr/lib/libpcp_trace.so.1
OPTS_32 = -o32 -mips2
#else
LIBPCP2 = /usr/lib32/libpcp.so.2
LIBPMDA2 = /usr/lib32/libpcp_pmda.so.2
LIBTRACE = /usr/lib32/libpcp_trace.so.1
OPTS_32 = -n32 -mips3
#endif

MIPSPROVERS	!= cc -version 2>&1 | sed -e 's/MIPSpro Compilers: Version //'
PID		!= sh -c 'echo $$$$'
HAVE_DYNAMIC	!= grep DYNAMIC /var/pcp/pmns/stdpmid >/dev/null && echo 1; exit 0

MPI_HEADER 	= /usr/include/mpi.h
MPI_PCP_HEADER	= /usr/include/pcp/mpi_pcp.h
HAVE_MPI	!= [ -f $(MPI_HEADER) -a -f $(MPI_PCP_HEADER) -a $(IRIX) != "IRIX6_2" ] && echo 1; exit 0
HAVE_COMPAT	!= [ -d /usr/include/pcp1.x ] && echo 1; exit 0

LITE_LDLIBS	= -lpcp

#DBG    = -DPCP_DEBUG -DDEBUG -DMALLOC_AUDIT -g
DBG     = -DPCP_DEBUG -DDEBUG -g
#DBG    = -DPCP_DEBUG -DDEBUG

DEBUG   = $(DBG) -D$(IRIX)=1

# when DEBUG includes -g, turn off optimization
#if !empty(DEBUG:M*-g*)
OPTIMIZER	= -O0
#endif

# supress these error numbers (comma-separated list)
DIAG_SUPPRESS	=
#if $(MIPSPROVERS) == "7.4"
DIAG_SUPPRESS	= -diag_suppress 1253,3604,3625
#endif
#   some build recipies do not honour the LWOFF variable:
#   LWOFF	= ,1253,3604,3625
#   so that's why DIAG_SUPPRESS exists

SRCARCH = mirage.0 moomba.client.0 babylon.percpu.0 omen.client.0 \
	bozo.client.0 late.0 ok-bigbin.0 ok-mv-bigbin.0 ok-noti-bigbin.0 \
	babylon.pmview.0 ok-foo.0 ok-mv-foo.0 bug.0 ace.0 951127.23.01.0 \
	bigace.0 960624.08.17.0 rattle.0 mark-bug.0 arch_a.0 arch_b.0 \
	err_v1.0 changeinst.0 19970807.09.54.0 19970807.09.59.0 \
	binning.0 wrap.0 573184.0 xbug1.0 pcpcmd.0 markmerge.0 \
	sample-secs.0 gmt-boring.0 kenj-pc-1.0 kenj-pc-2.0

MKARCH	= foo.0 mv-foo.0 noti-foo.0 bar.0 mv-bar.0 noti-bar.0 \
	bigbin.0 mv-bigbin.0 noti-bigbin.0 truncbin.0 null.0 \
	interp.0 mv-interp.0 noti-interp.0 noti-ok-foo.0 gap.0 gap2.0 \
	dodgey-some.0 dodgey-all.0 dodgey-mixed.0

ARCH	= $(SRCARCH) $(MKARCH)

# Try and keep this initial CFILES block the same in Makefile
# and GNUmakefile for all common apps that are used in both IRIX
# and Linux

CFILES	= pducheck.c disk_test.c exercise.c chkoptfetch.c \
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
	pmpost-exploit.c multifetch.c context_test.c

#if !empty(HAVE_DYNAMIC)
# rest of original list from ../src for reference
#CFILES	+= dynamic_pmda.c
#endif

TARGETS	= \
	$(CFILES:R:S/broken_pmda//) \
	pminfo_lite chkctx2_lite torture_api.64 pmclient template \
	fetchrate_lite pv pv64 pmval \
	broken_pmda_2_0 broken_pmda_2_1 broken_pmda_2_2 \
	broken_pmda_2_3 broken_pmda_2_4 broken_pmda_2_5 \
	broken_v2.dir \
	pmsocks_objstyle_32 pmsocks_objstyle_n32 pmsocks_objstyle_64

#if $(HAVE_COMPAT)
TARGETS	+= broken_pmda_1_0 broken_pmda_1_1 broken_pmda_1_2 \
	broken_pmda_1_3 broken_pmda_1_4 broken_pmda_1_5
#endif

LCOPTS	= -fullwarn $(DIAG_SUPPRESS) -I/usr/include/pcp -MDupdate Makedepend
LCOPTS32	= -fullwarn -I/usr/include/pcp -MDupdate Makedepend
LCDEFS	= $(DEBUG) $(HAVE_DEV_IN_LIBPCP)
LLDOPTS = 
LLDLIBS	= -lpcp

LC++OPTS = -fullwarn $(DIAG_SUPPRESS) -I/usr/include/pcp -MDupdate Makedepend
LC++DEFS = $(DEBUG)
LC++INCS =

#if $(IRIX) == "IRIX6_2"
LC++OPTS += $(OPTS_32)
#else
LC++OPTS += -n32
#endif

# all archives, except the ones checked into the source tree,
# then add executables beyond $(TARGETS)
#
LDIRT	= *.log domain.h help.pag help.dir mv-foo.? mv-bar.? mv-bigbin.? \
	$(MKARCH) $(MKARCH:S/.0/.meta/g) $(MKARCH:S/.0/.index/g) \
	chkctx2_lite.c pmclient_src mv-interp.? fetchrate_lite.c \
	broken_v2.dir broken_v2.pag \
	broken_pmda_*.c broken.dir broken.pag broken_pmda_? real_broken_pmns \
	pv64.c pmsocks_objstyle_n32.c pmsocks_objstyle_64.c \
	localconfig.h

#if $(IRIX) != "IRIX6_2"
#LDIRT += libfake_mpi.c
#endif

.ORDER:	torture_api torture_api.other chkctx2 fetchrate

LIB64PCP = /usr/lib64/libpcp.so.2
LIBPCP1 = /usr/lib/libpcp.so.1
LIBPMDA1 = /usr/lib/libpcp_pmda.so.1
OPTS_V1 = -o32 -mips2

LCOPTS1 = -fullwarn -I/usr/include/pcp1.x -I/usr/include/pcp1.x/pcp -MDupdate Makedepend

default: $(CFILES) $(TARGETS) $(ARCH) # ipcs_clear

$(TARGETS):

include $(COMMONRULES)

install: default

# remove just the binaries, not the archives
#
clean-exec:
	rm -f $(TARGETS)

# remave just the archives, not the binaries
#
clean-arch:
	rm -f $(MKARCH) $(MKARCH:S/.0/.meta/g) $(MKARCH:S/.0/.index/g)

# make just the archives, not the binaries
#
arch:	$(ARCH)

# --- do NOT need libpcp
#

pause:	pause.c
	$(CC) $(LCDEFS) $(LCOPTS) -o pause pause.c

errmap: errmap.c
	$(CC) $(LCDEFS) $(LCOPTS) -o errmap errmap.c

sizeof: sizeof_32
	rm -f $@
	ln sizeof_32 sizeof

sizeof_32: sizeof.c
	$(CC) -32 $(LCDEFS) $(LCOPTS32) -o sizeof_32 sizeof.c

sizeof_n32: sizeof.c
	rm -f $@ $@.c
	ln -s sizeof.c $@.c
	$(CC) -n32 $(LCDEFS) $(LCOPTS) -o $@ $@.c

sizeof_64: sizeof.c
#if !empty(IRIX_OBJECT:M*64*)
	rm -f $@ $@.c
	ln -s sizeof.c $@.c
	$(CC) -64 -mips3  $(LCDEFS) $(LCOPTS) -o $@ $@.c
#else
	( echo "#!/bin/sh"; echo 'echo "sizeof_64 - not QAing on this platform"' ) >sizeof_64
	chmod 755 sizeof_64
#endif

pmsocks_objstyle: pmsocks_objstyle_32
	rm -f $@
	ln pmsocks_objstyle_32 pmsocks_objstyle

pmsocks_objstyle_32: pmsocks_objstyle.c
	$(CC) -32 $(LCDEFS) $(LCOPTS32) -o pmsocks_objstyle_32 pmsocks_objstyle.c -lpcp_dev -lpcp

pmsocks_objstyle_n32: pmsocks_objstyle.c
	rm -f $@ $@.c
	ln -s pmsocks_objstyle.c $@.c
	$(CC) -n32 $(LCDEFS) $(LCOPTS) -o $@ $@.c -lpcp_dev -lpcp

pmsocks_objstyle_64: pmsocks_objstyle.c
#if !empty(IRIX_OBJECT:M*64*)
	rm -f $@ $@.c
	ln -s pmsocks_objstyle.c $@.c
	$(CC) -64 -mips3  $(LCDEFS) $(LCOPTS) -o $@ $@.c -lpcp_dev -lpcp
#else
	( echo "#!/bin/sh"; echo 'echo "pmsocks_objstyle_64 - not QAing on this platform"' ) >pmsocks_objstyle_64
	chmod 755 pmsocks_objstyle_64
#endif

595:	595.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ 595.c

sproc_mem_share:	sproc_mem_share.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ sproc_mem_share.c

cpuload:	cpuload.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ cpuload.c

load_o2000:	load_o2000.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ load_o2000.c

chain:	chain.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chain.c

syscall:	syscall.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ syscall.c

getpagesize:	getpagesize.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ getpagesize.c

rdwr:	rdwr.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ rdwr.c

spawn:	spawn.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ spawn.c

diowr:	diowr.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ diowr.c

endian:	endian.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ endian.c

pmlcmacro:	pmlcmacro.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ pmlcmacro.c

sys5_ipc:	sys5_ipc.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ sys5_ipc.c

stripmark: stripmark.c
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ stripmark.c

tohex: tohex.c
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ tohex.c

# --- need libpcp
#

template:	template.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ template.c $(LLDLIBS)

# --- need libpcp_dev ...
#     this is in pcp_noship.sw.eoe which must be concurrently installed
#

pmie-lic:	pmie-lic.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ pmie-lic.c -lpcp_dev -llmsgi $(LLDLIBS)

exercise:	exercise.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ exercise.c -lpcp_dev $(LLDLIBS)

chkacc1:	chkacc1.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chkacc1.c -lpcp_dev $(LLDLIBS)

chkacc2:	chkacc2.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chkacc2.c -lpcp_dev $(LLDLIBS)

chkacc3:	chkacc3.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chkacc3.c -lpcp_dev $(LLDLIBS)

ipc:	ipc.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ ipc.c -lpcp_dev $(LLDLIBS)

auth_client:	auth_client.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ auth_client.c -lpcp_dev $(LLDLIBS)

fakebrand:	fakebrand.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ fakebrand.c -lpcp_dev -llmsgi $(LLDLIBS)

chk_memleak:	chk_memleak.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chk_memleak.c -lpcp $(LLDLIBS)

chknumval:	chknumval.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ chknumval.c -lpcp_dev $(LLDLIBS)

logcontrol:	logcontrol.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ logcontrol.c -lpcp_dev $(LLDLIBS)

datax:	datax.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ datax.c -lpcp_dev $(LLDLIBS)

pmcdgone:	pmcdgone.c localconfig.h
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ pmcdgone.c -lpcp_dev $(LLDLIBS)

pmbrand_qa:	pmbrand.c
	rm -f $@
	$(CC) -DQA=1 $(LCDEFS) $(LCOPTS) -o $@ pmbrand.c -lpcp_dev -llmsgi $(LLDLIBS)

slow_af:	slow_af.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ slow_af.c -lpcp_dev $(LLDLIBS)

pmval:	pmval.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ $@.c -lpcp_dev $(LLDLIBS) -lm

# --- need libpcp but also PM_CONTEXT_LOCAL
#

fetchrate_lite:	fetchrate_lite.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DLITE=1 $(LCOPTS) -o $@ fetchrate_lite.c $(LLDOPTS) $(LITE_LDLIBS)

fetchrate_lite.c:	fetchrate.c
	rm -f $@
	ln -s fetchrate.c fetchrate_lite.c

chkctx2_lite:	chkctx2_lite.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DLITE=1 $(LCOPTS) -o $@ chkctx2_lite.c $(LLDOPTS) $(LITE_LDLIBS)

chkctx2_lite.c:	chkctx2.c
	rm -f $@
	ln -s chkctx2.c chkctx2_lite.c

hp-mib: hp-mib.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DLITE=1 $(LCOPTS) -o $@ hp-mib.c $(LLDOPTS) $(LITE_LDLIBS)

pcp_lite_crash: pcp_lite_crash.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DLITE=1 $(LCOPTS) -o $@ pcp_lite_crash.c $(LLDOPTS) $(LITE_LDLIBS)

store: store.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DLITE=1 $(LCOPTS) -o $@ store.c $(LLDOPTS) $(LITE_LDLIBS)

pminfo_lite:	pminfo.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) -DPM_USE_CONTEXT_LOCAL=1 $(LCOPTS) -o $@ pminfo.c -lpcp_dev $(LLDOPTS) $(LITE_LDLIBS)

# --- need libpcp_pmda
#

domain.h:	/var/pcp/pmns/stdpmid
	rm -f $@
	echo "/*" >domain.h
	echo " * built from /var/pcp/pmns/stdpmid on `date`" >>domain.h
	echo " */" >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]BROKEN[ 	]/s/.*[ 	]/#define BROKEN /p' /var/pcp/pmns/stdpmid >>domain.h
	sed -n -e 's/[ 	]*$$//' -e '/define[ 	]DYNAMIC[ 	]/s/.*[ 	]/#define DYNAMIC /p' /var/pcp/pmns/stdpmid >>domain.h

broken_v2.dir:	broken_help real_broken_pmns
	rm -f broken_v2.pag broken_v2.dir
	/usr/pcp/bin/newhelp -n real_broken_pmns -v 2 -o broken_v2 broken_help

real_broken_pmns:	domain.h broken_pmns
	cat domain.h >real_broken_pmns
	echo >>real_broken_pmns
	echo 'root {' >>real_broken_pmns
	echo '  broken' >>real_broken_pmns
	echo '}' >>real_broken_pmns
	echo >>real_broken_pmns
	cat broken_pmns >>real_broken_pmns

broken_pmda_1_0: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_1_1: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -DBUG_1 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_1_2: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -DBUG_2 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_1_3: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -DBUG_3 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_1_4: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -DBUG_4 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_1_5: broken_pmda.v1.c domain.h
	rm -f $@.c
	ln -s broken_pmda.v1.c $@.c
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -DVERSION_1 -DBUG_5 -o $@ $@.c $(LLDOPTS) $(LIBPMDA1) $(LIBPCP1)

broken_pmda_2_0: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

broken_pmda_2_1: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -DBUG_1 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

broken_pmda_2_2: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -DBUG_2 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

broken_pmda_2_3: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -DBUG_3 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

broken_pmda_2_4: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -DBUG_4 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

broken_pmda_2_5: broken_pmda.c domain.h
	rm -f $@ $@.c
	ln -s broken_pmda.c $@.c
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -DBUG_5 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

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
	/usr/pcp/bin/newhelp -n real_dynamic_pmns -v 2 -o dynamic dynamic_help

dynamic_pmda: dynamic_pmda.c domain.h real_dynamic_pmns
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -DVERSION_2 -o $@ $@.c $(LLDOPTS) -lpcp_pmda $(LLDLIBS)

# --- special cases - hand-crafted libraries required
#

xpmval: xpmval.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ xpmval.c $(LLDOPTS) -lm $(LLDLIBS)

numberstr: numberstr.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ numberstr.c $(LLDOPTS) -lgen $(LLDLIBS)

read-bf: read-bf.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ read-bf.c $(LLDOPTS) -lgen $(LLDLIBS)

write-bf: write-bf.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ write-bf.c $(LLDOPTS) -lgen $(LLDLIBS)

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
pducheck:	pducheck.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ pducheck.c -lpcp_dev -lpcp_trace $(LLDLIBS)

pdu-server:	pdu-server.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ pdu-server.c -lpcp_dev -lpcp_trace $(LLDLIBS)

obs:	obs.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ obs.c -lpcp_trace 

torture_trace:	torture_trace.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ torture_trace.c -lpcp_trace 

tstate:	tstate.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ tstate.c -lpcp_trace 

tabort:	tabort.c
	rm -f $@
	$(CC) $(LCDEFS) $(LCOPTS) -o $@ tabort.c -lpcp_trace 

# --- $(IRIX_OBJECT) format dependencies
#

#if !empty(IRIX_OBJECT:M*64*)
pv64: pv.c
	rm -f $@ $@.c
	ln -s pv.c $@.c
	$(CC) -64 -mips3 $(LCDEFS) $(LCOPTS) -o $@ $@.c $(LLDOPTS) -lpcp
#else
pv64:
	rm -f $@
	ln -s pv pv64
#endif

# --- use 1.x headers and libraries
#

pmdumplog_v1:	pmdumplog_v1.c
	rm -f $@
	$(CC) $(OPTS_V1) $(LCDEFS) $(LCOPTS1) -o $@ $@.c /usr/lib/libpcp_util.a $(LIBPCP1)

# --- torture_api executables
#

#if !empty(IRIX_OBJECT:M*64*)
torture_api.64: torture_api.c
	$(CC) -64 -mips3  $(LCDEFS) $(LCOPTS) -o $@ torture_api.c $(LIB64PCP)
#else
torture_api.64:
#endif
	
xyz:
	@echo LDOPTS=$(LDOPTS)
	@echo LLDOPTS=$(LLDOPTS)
	@echo GLDOPTS=$(GLDOPTS)

# -- archives
#

foo.0:	config.foo
	rm -f foo.index foo.meta foo.0
	/usr/pcp/bin/pmlogger -c config.foo -s 8 foo

mv-foo.0:	config.foo
	rm -f mv-foo.index mv-foo.meta mv-foo.?
	/usr/pcp/bin/pmlogger -c config.foo -s 8 -v 3 mv-foo

noti-foo.0:	foo.0
	rm -f noti-foo.0 noti-foo.meta
	cp foo.0 noti-foo.0
	cp foo.meta noti-foo.meta

noti-ok-foo.0:	ok-foo.0
	rm -f noti-ok-foo.0 noti-ok-foo.meta
	cp ok-foo.0 noti-ok-foo.0
	cp ok-foo.meta noti-ok-foo.meta

bar.0:	config.bar
	rm -f bar.index bar.meta bar.0
	/usr/pcp/bin/pmlogger -c config.bar -s 70 bar

mv-bar.0:	config.bar
	rm -f mv-bar.index mv-bar.meta mv-bar.?
	/usr/pcp/bin/pmlogger -c config.bar -s 70 -v 20 mv-bar

noti-bar.0:	bar.0
	rm -f noti-bar.0 noti-bar.meta
	cp bar.0 noti-bar.0
	cp bar.meta noti-bar.meta

mirage.0:
	rm -f mirage.index mirage.meta mirage.0
	/usr/pcp/bin/pmlogger -c config.mirage -s 20 mirage

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
	/usr/pcp/bin/pmlogger -c config.bigbin -s 1000 bigbin

mv-bigbin.0:	config.bigbin
	rm -f mv-bigbin.index mv-bigbin.meta mv-bigbin.?
	sync
	sleep 3
	/usr/pcp/bin/pmlogger -c config.bigbin -s 1000 -v 101 mv-bigbin

noti-bigbin.0:	bigbin.0
	cp bigbin.0 noti-bigbin.0
	cp bigbin.meta noti-bigbin.meta

interp.0:	config.interp
	rm -f interp.index interp.meta interp.0
	/usr/pcp/bin/pmlogger -c config.interp -s 30 interp

mv-interp.0:	config.interp
	rm -f mv-interp.index mv-interp.meta mv-interp.?
	/usr/pcp/bin/pmlogger -c config.interp -s 30 -v 10 mv-interp

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
	echo 'log mandatory on once { hinv.ncpu }' >tmp/$(PID).A.config
	echo 'log mandatory on 1sec {' >>tmp/$(PID).A.config
	echo '  sample.seconds,hinv.ncpu,sample.bin["bin-100","bin-200","bin-400"]' >>tmp/$(PID).A.config
	echo '  sample.drift' >>tmp/$(PID).A.config
	echo '  irix.network.interface.total.packets["lo0","ec0"]' >>tmp/$(PID).A.config
	echo '}' >>tmp/$(PID).A.config
	echo 'log mandatory on once { sample.bin["bin-500"] }' >tmp/$(PID).B.config
	echo 'log mandatory on 1sec {' >>tmp/$(PID).B.config
	echo '  sample.seconds,sample.milliseconds,sample.bin["bin-100","bin-300","bin-400"]' >>tmp/$(PID).B.config
	echo '}' >>tmp/$(PID).B.config
	echo 'log mandatory on once { hinv.ncpu }' >tmp/$(PID).C.config
	echo 'log mandatory on 1sec {' >>tmp/$(PID).C.config
	echo '  sample.drift' >>tmp/$(PID).C.config
	echo '  sample.seconds,sample.bin["bin-200","bin-300","bin-400","bin-500"]' >>tmp/$(PID).C.config
	echo '  irix.network.interface.total.packets["ec2","lo0"]' >>tmp/$(PID).C.config
	echo '}' >>tmp/$(PID).C.config
	/usr/pcp/bin/pmlogger -s 5 -c tmp/$(PID).A.config tmp/$(PID).A
	sleep 3
	/usr/pcp/bin/pmlogger -s 5 -c tmp/$(PID).B.config tmp/$(PID).B
	sleep 3
	/usr/pcp/bin/pmlogger -s 5 -c tmp/$(PID).C.config tmp/$(PID).C
	rm -f changeinst.index changeinst.meta changeinst.0
	/usr/pcp/bin/pmlogmerge tmp/$(PID).A tmp/$(PID).B tmp/$(PID).C changeinst
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
	/usr/pcp/bin/pmlogger -s 10 -c tmp/A.config tmp/A
	sleep 5
	/usr/pcp/bin/pmlogger -s 10 -c tmp/B.config tmp/B
	sleep 5
	/usr/pcp/bin/pmlogger -s 10 -c tmp/C.config tmp/C
	rm -f gap.index gap.meta gap.0
	/usr/pcp/bin/pmlogextract tmp/A tmp/B gap
	rm -f gap2.index gap2.meta gap2.0
	/usr/pcp/bin/pmlogextract tmp/A tmp/B tmp/C gap2
	rm -rf tmp

pmclient:	pmclient_src
	rm -f pmclient pmclient_src/pmclient
	cd pmclient_src ; make pmclient
	ln pmclient_src/pmclient pmclient

pmclient_src: /var/pcp/demos/pmclient
	- rm -rf pmclient_src
	mkdir pmclient_src
	cp /var/pcp/demos/pmclient/* pmclient_src
	cd pmclient_src ; make clobber

torture_indom: torture_indom.c
	rm -f $@
	$(CC) $(IRIX_OBJECT) $(LCDEFS) $(LCOPTS) -o $@ torture_indom.c $(LLDOPTS) $(LLDLIBS)

# --- miscellaneous
#
getpid.so:	getpid.o
	$(LD) -shared -o getpid.so -nostdlib getpid.o

arch_maxfd.o:	localconfig.h
scale.o:	localconfig.h

localconfig.h:
	cd ..; ./mk.localconfig && cp localconfig.h src-oss
