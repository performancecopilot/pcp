TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

VIEWDIR = $(PCP_VAR_DIR)/config/pmchart
VIEWS	= CPU Disk Diskbytes Loadavg NFS2 NFS3 Filesystem Memory Netbytes \
	  Netpackets PMCD Syscalls Paging Overview Schemes Sockets Swap \
	  ApacheServer ElasticsearchServer

LSRCFILES = $(VIEWS)

default build-me:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(VIEWDIR)
	$(INSTALL) -m 0444 $(VIEWS) $(VIEWDIR)

default_pcp:	default

install_pcp:	install
