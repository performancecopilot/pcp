TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

VIEWS	= CPU Disk Diskbytes Loadavg NFS2 NFS3 Filesystem Memory Netbytes \
	  Netpackets PMCD Syscalls Paging Overview Schemes Sockets Swap \
	  ApacheServer ElasticsearchServer vCPU MemAvailable

default build-me:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 644 $(VIEWS) $(PCP_VAR_DIR)/config/pmchart

default_pcp:	default

install_pcp:	install
