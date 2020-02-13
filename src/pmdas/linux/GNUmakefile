#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013-2019 Red Hat.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

IAM		= linux
DOMAIN		= LINUX
CMDTARGET	= pmdalinux
LIBTARGET	= pmda_linux.so
PMDAINIT	= linux_init
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
LOGREWRITEDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE	= "linux	60	pipe	binary		$(PMDADIR)/$(CMDTARGET)"

CFILES		= pmda.c linux_table.c mem_bandwidth.c namespaces.c \
		  proc_stat.c proc_meminfo.c proc_loadavg.c \
		  proc_net_dev.c interrupts.c filesys.c ipc.c \
		  swapdev.c proc_net_rpc.c proc_partitions.c \
		  getinfo.c proc_net_sockstat.c proc_net_snmp.c \
		  proc_scsi.c proc_cpuinfo.c proc_net_tcp.c \
		  proc_sys_fs.c proc_sys_kernel.c login.c ksm.c \
		  proc_slabinfo.c proc_uptime.c proc_vmstat.c \
		  proc_locks.c sysfs_kernel.c numa_meminfo.c \
		  proc_net_netstat.c proc_net_softnet.c \
		  proc_net_raw.c proc_net_udp.c proc_net_unix.c \
		  proc_net_snmp6.c proc_buddyinfo.c proc_zoneinfo.c \
		  sysfs_tapestats.c proc_net_sockstat6.c \
		  proc_fs_nfsd.c proc_tty.c proc_pressure.c

HFILES		= linux.h linux_table.h convert.h namespaces.h \
		  proc_stat.h proc_meminfo.h proc_loadavg.h \
		  proc_net_dev.h interrupts.h filesys.h ipc.h \
		  swapdev.h proc_net_rpc.h proc_partitions.h \
		  getinfo.h proc_net_sockstat.h proc_net_snmp.h \
		  proc_scsi.h proc_cpuinfo.h proc_net_tcp.h \
		  proc_sys_fs.h proc_sys_kernel.h login.h ksm.h \
		  proc_slabinfo.h proc_uptime.h proc_vmstat.h \
		  proc_locks.h sysfs_kernel.h numa_meminfo.h \
		  proc_net_netstat.h proc_net_softnet.h \
		  proc_net_raw.h proc_net_udp.h proc_net_unix.h \
		  proc_net_snmp6.h proc_buddyinfo.h proc_zoneinfo.h \
		  sysfs_tapestats.h proc_net_sockstat6.h \
		  proc_fs_nfsd.h proc_tty.h proc_pressure.h

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
CONFTARGETS	= linux_kernel_ulong.conf \
		  linux_kernel_fixups.conf \
		  proc_net_sockstat_deprecate.conf \
		  proc_fs_nfsd_fixups.conf \

LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT) $(CONFTARGETS)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

# Uncomment these flags for profiling
# LCFLAGS	+= -pg
# LLDFLAGS	+= -pg

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS) $(CONFTARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help $(HELPTARGETS) $(PMDADIR)
	$(INSTALL) -m 644 bandwidth.conf $(PMDADIR)/samplebandwidth.conf
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_linux $(PCP_VAR_DIR)/pmns/root_linux
	$(INSTALL) -m 644 proc_fs_nfsd_fixups.conf $(LOGREWRITEDIR)/linux_proc_fs_nfsd_fixups.conf
	$(INSTALL) -m 644 proc_net_snmp_migrate.conf $(LOGREWRITEDIR)/linux_proc_net_snmp_migrate.conf
	$(INSTALL) -m 644 proc_net_tcp_migrate.conf $(LOGREWRITEDIR)/linux_proc_net_tcp_migrate.conf
	$(INSTALL) -m 644 linux_kernel_ulong.conf linux_kernel_fixups.conf $(LOGREWRITEDIR)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(NEWHELP) -n root_linux -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(CONFTARGETS):	mk.rewrite
	CPP="$(CPP)" INCDIR="$(TOPDIR)/src/include" ./mk.rewrite

check:: $(CFILES) $(HFILES)
	$(CLINT) $^

interrupts.o pmda.o proc_partitions.o:	linux.h
numa_meminfo.o proc_cpuinfo.o proc_stat.o:	linux.h
filesys.o interrupts.o pmda.o:	filesys.h
pmda.o:	getinfo.h
pmda.o ipc.o:	ipc.h
pmda.o login.o:	login.h
interrupts.o pmda.o:	interrupts.h
linux_table.o numa_meminfo.o pmda.o:	linux_table.h
numa_meminfo.o pmda.o:	numa_meminfo.h
pmda.o proc_cpuinfo.o proc_stat.o:	proc_cpuinfo.h
pmda.o proc_loadavg.o:	proc_loadavg.h
pmda.o proc_locks.o:	proc_locks.h
pmda.o proc_meminfo.o:	proc_meminfo.h
pmda.o proc_net_dev.o:	proc_net_dev.h
pmda.o proc_net_raw.o:	proc_net_raw.h
pmda.o proc_net_tcp.o:	proc_net_tcp.h
pmda.o proc_net_udp.o:	proc_net_udp.h
pmda.o proc_net_unix.o:	proc_net_unix.h
pmda.o proc_net_netstat.o:	proc_net_netstat.h
pmda.o proc_net_rpc.o:	proc_net_rpc.h
pmda.o proc_net_snmp.o:	proc_net_snmp.h
pmda.o proc_net_sockstat.o:	proc_net_sockstat.h
pmda.o proc_net_sockstat6.o:	proc_net_sockstat6.h
pmda.o proc_partitions.o:	proc_partitions.h
pmda.o proc_scsi.o:	proc_scsi.h
pmda.o proc_slabinfo.o:	proc_slabinfo.h
pmda.o proc_stat.o:	proc_stat.h
pmda.o proc_sys_fs.o:	proc_sys_fs.h
pmda.o proc_sys_kernel.o:	proc_sys_kernel.h
pmda.o proc_uptime.o:	proc_uptime.h
pmda.o proc_vmstat.o:	proc_vmstat.h
pmda.o swapdev.o:	swapdev.h
pmda.o sysfs_kernel.o:	sysfs_kernel.h
pmda.o proc_buddyinfo.o:	proc_buddyinfo.h
pmda.o proc_zoneinfo.o:	proc_zoneinfo.h
pmda.o ksm.o:	ksm.h
pmda.o proc_fs_nfsd.o:	proc_fs_nfsd.h
pmda.o:	$(VERSION_SCRIPT)
