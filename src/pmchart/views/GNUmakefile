TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

VIEWS	= CPU Disk Diskbytes Loadavg NFS2 NFS3 Filesystem Memory Netbytes \
	  Netpackets PMCD Syscalls Paging Overview Schemes Sockets Swap \
	  ApacheServer ElasticsearchServer vCPU MemAvailable Iostat Iostat.DM \
	  Iostat.MD
	  
EXEC_VIEWS	= BusyCPU

CONFIGETC	= $(PCP_SYSCONF_DIR)/pmchart
CONFIGVAR	= $(PCP_VAR_DIR)/config/pmchart

default build-me:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(CONFIGETC)
	$(INSTALL) -m 755 -d $(CONFIGVAR)
	$(INSTALL) -m 644 -t $(CONFIGVAR) $(VIEWS) $(CONFIGETC)
	$(INSTALL) -m 755 -t $(CONFIGVAR)/$(EXEC_VIEWS) $(EXEC_VIEWS) $(CONFIGETC)/$(EXEC_VIEWS)

default_pcp:	default

install_pcp:	install
