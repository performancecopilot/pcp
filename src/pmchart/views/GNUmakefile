TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

VIEWS	= CPU Disk Diskbytes Loadavg NFS2 NFS3 Filesystem Memory Netbytes \
	  Netpackets PMCD Syscalls Paging Overview Schemes Sockets Swap \
	  ApacheServer ElasticsearchServer vCPU MemAvailable Iostat Iostat.DM \
	  Iostat.MD
	  
EXEC_VIEWS	= BusyCPU

default build-me:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 644 $(VIEWS) $(PCP_VAR_DIR)/config/pmchart
	$(INSTALL) -m 755 $(EXEC_VIEWS) $(PCP_VAR_DIR)/config/pmchart/$(EXEC_VIEWS)

default_pcp:	default

install_pcp:	install
