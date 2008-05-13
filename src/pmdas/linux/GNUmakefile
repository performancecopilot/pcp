#! gmake
#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
#*************************************************************************
#
# Makefile - linux PMDA (shared Dobject)
#
#*************************************************************************

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

LIBTARGET	= pmda_linux.so
CMDTARGET	= pmdalinux

CFILES		= pmda.c proc_stat.c proc_meminfo.c proc_loadavg.c \
		  proc_net_dev.c proc_interrupts.c filesys.c \
		  swapdev.c proc_net_rpc.c proc_pid.c proc_partitions.c \
		  ttyname.c proc_net_sockstat.c proc_runq.c \
		  proc_net_snmp.c proc_scsi.c proc_fs_xfs.c \
		  proc_cpuinfo.c proc_net_tcp.c \
		  proc_slabinfo.c sem_limits.c msg_limits.c shm_limits.c \
		  proc_uptime.c ksym.c proc_sys_fs.c proc_vmstat.c \
		  infiniband.c

HFILES		= domain.h proc_stat.h proc_meminfo.h proc_loadavg.h \
		  proc_net_dev.h proc_interrupts.h filesys.h swapdev.h \
		  proc_net_rpc.h proc_pid.h proc_partitions.h \
		  proc_net_sockstat.h proc_runq.h proc_net_snmp.h \
		  proc_scsi.h proc_fs_xfs.h \
		  proc_cpuinfo.h proc_net_tcp.h proc_slabinfo.h \
		  sem_limits.h msg_limits.h shm_limits.h proc_uptime.h \
		  ksym.h proc_sys_fs.h proc_vmstat.h clusters.h indom.h \
		  infiniband.h convert.h

LSRCFILES 	= help root_linux root_irix

# Uncomment these flags for profiling, then use pmdalinux rather than the dso
# (you will need to change the linux line in $PCP_PMCDCONF_PATH)
# LCFLAGS = -pg
# LLDFLAGS = -pg
LLDLIBS	= -lpcp_pmda -lpcp -lpthread

LDIRT		= help.dir help.pag

PMDADIR = $(PCP_PMDAS_DIR)/linux
CONF_LINE = "linux	60	dso	linux_init	$(PCP_PMDAS_DIR)/linux/pmda_linux.so"

default: $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

include $(BUILDRULES)

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_linux -v 2 -o help < help

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_linux $(PCP_VAR_DIR)/pmns/root_linux

default_pcp : default

install_pcp : install
