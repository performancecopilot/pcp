#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/cisco

CISCOS = ATM-12-0 Ethernet-0 Ethernet-1-0 Ethernet-3-5 \
	 FastEthernet-0-5 FastEthernet-0-6 FastEthernet-1-0 \
	 Fddi-2-0 GigabitEthernet-0-1 Hssi-3-0 hugh-800 \
	 Serial-0 Serial-1 Serial-1-0-min Serial-1-0-sec \
	 Vlan-1 Vlan-160 Vlan-256

LSRCFILES = $(CISCOS)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CISCOS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
