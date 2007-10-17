TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
include /etc/pcp.conf
VIEWDIR = $(PCP_VAR_DIR)/config/kmchart
VIEWS	= Apache CPU Disk Loadavg NFS2 NFS3 Filesystem Netbytes Netpackets \
	  PMCD Syscalls Paging Overview Sockets Swap Cisco Sendmail \
	  ShpingCPU ShpingElapsed Schemes

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(VIEWDIR)
	$(INSTALL) -m 0444 $(VIEWS) $(VIEWDIR)
