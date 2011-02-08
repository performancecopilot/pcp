#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
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
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "linux	60	dso	linux_init	$(PMDADIR)/$(LIBTARGET)"

CFILES		= pmda.c proc_stat.c proc_meminfo.c proc_loadavg.c \
		  proc_net_dev.c interrupts.c filesys.c \
		  swapdev.c proc_net_rpc.c proc_pid.c proc_partitions.c \
		  getinfo.c proc_net_sockstat.c proc_runq.c \
		  proc_net_snmp.c proc_scsi.c proc_fs_xfs.c \
		  proc_cpuinfo.c proc_net_tcp.c \
		  proc_slabinfo.c sem_limits.c msg_limits.c shm_limits.c \
		  proc_uptime.c ksym.c proc_sys_fs.c proc_vmstat.c \
		  sysfs_kernel.c linux_table.c numa_meminfo.c \
		  dynamic.c cgroups.c

HFILES		= proc_stat.h proc_meminfo.h proc_loadavg.h \
		  proc_net_dev.h interrupts.h filesys.h swapdev.h \
		  proc_net_rpc.h proc_pid.h proc_partitions.h getinfo.h \
		  proc_net_sockstat.h proc_runq.h proc_net_snmp.h \
		  proc_scsi.h proc_fs_xfs.h \
		  proc_cpuinfo.h proc_net_tcp.h proc_slabinfo.h \
		  sem_limits.h msg_limits.h shm_limits.h proc_uptime.h \
		  ksym.h proc_sys_fs.h proc_vmstat.h clusters.h indom.h \
		  convert.h sysfs_kernel.h linux_table.h numa_meminfo.h \
		  dynamic.h cgroups.h

LSRCFILES 	= help root_linux
LDIRT		= help.dir help.pag domain.h

LLDLIBS		= -lpcp_pmda -lpcp

# Uncomment these flags for profiling
# LCFLAGS	= -pg
# LLDFLAGS	= -pg

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_linux $(PCP_VAR_DIR)/pmns/root_linux
else
build-me:
install:
endif

help.dir help.pag : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_linux -v 2 -o help < help

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	rm -f domain.h
	@echo "/*" >domain.h
	@echo " * built from $<" >>domain.h
	@echo " */" >>domain.h
	@$(PCP_AWK_PROG) <$< '\
$$1=="#define" && $$2 == "$(DOMAIN)" { print "#define $(DOMAIN) " $$3 >>"$@"; found++ }\
END	{ if (found == 0) { print "Botch: no #define for domain $(DOMAIN) in $<";  exit(1) }\
          if (found > 1) { print "Botch: multiple #defines for domain $(DOMAIN) in $<"; print "... see $@ for details"; exit(1) }\
	  exit(0)\
	}'
