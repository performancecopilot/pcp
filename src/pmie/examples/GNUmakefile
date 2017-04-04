#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
include $(TOPDIR)/src/include/builddefs

UAG_SOURCE	= uag.head $(sort $(wildcard uag.[0-9][0-9]))
UPM_SOURCE	= upm.head $(sort $(wildcard upm.[0-9][0-9]))
CPU_SOURCE	= cpu.head $(sort $(wildcard cpu.[0-9][0-9]))
DISK_SOURCE	= disk.head $(sort $(wildcard disk.[0-9][0-9]))
FILESYS_SOURCE	= filesys.head $(sort $(wildcard filesys.[0-9][0-9]))
RAS_SOURCE	= ras.head $(sort $(wildcard ras.[0-9][0-9]))
SWAP_SOURCE	= swap.head $(sort $(wildcard swap.[0-9][0-9]))
NETWORK_SOURCE	= network.head $(sort $(wildcard network.[0-9][0-9]))
ENVIRON_SOURCE	= environ.head $(sort $(wildcard environ.[0-9][0-9]))
WEBREPORT_SOURCE= webreport.head $(sort $(wildcard webreport.[0-9][0-9]))

TARGETS		= UAG UPM CPU DISK FILESYS RAS SWAP NETWORK ENVIRON \
		  WEBREPORT

LDIRT           = $(TARGETS)

LSRCFILES = README $(UAG_SOURCE) $(UPM_SOURCE) $(CPU_SOURCE) \
        $(DISK_SOURCE) $(FILESYS_SOURCE) $(RAS_SOURCE) $(SWAP_SOURCE) \
        $(NETWORK_SOURCE) $(ENVIRON_SOURCE) $(WEBREPORT_SOURCE)
EX_DIR =  $(PCP_SHARE_DIR)/examples/pmie

default:	$(TARGETS) README

install:	default
	$(INSTALL) -m 755 -d $(EX_DIR)
	$(INSTALL) -m 644 $(TARGETS) README $(EX_DIR)

UAG: $(UAG_SOURCE)
	rm -f UAG
	for file in $(UAG_SOURCE); do cat $$file >>UAG; echo >>UAG; done

UPM: $(UPM_SOURCE)
	rm -f UPM
	for file in $(UPM_SOURCE); do cat $$file >>UPM; echo >>UPM; done

CPU: $(CPU_SOURCE)
	rm -f CPU
	for file in $(CPU_SOURCE); do cat $$file >>CPU; echo >>CPU; done

DISK: $(DISK_SOURCE)
	rm -f DISK DISK.in
	for file in $(DISK_SOURCE); do cat $$file >>DISK.in; echo >>DISK.in; done
	sed -e "s@/usr/pcp/bin/pmpost@$(PCP_BINADM_DIR)/pmpost@" <DISK.in >DISK
	rm -f DISK.in

FILESYS: $(FILESYS_SOURCE)
	rm -f FILESYS
	for file in $(FILESYS_SOURCE); do cat $$file >>FILESYS; echo >>FILESYS; done

RAS: $(RAS_SOURCE)
	rm -f RAS
	for file in $(RAS_SOURCE); do cat $$file >>RAS; echo >>RAS; done

SWAP: $(SWAP_SOURCE)
	rm -f SWAP
	for file in $(SWAP_SOURCE); do cat $$file >>SWAP; echo >>SWAP; done

NETWORK: $(NETWORK_SOURCE)
	rm -f NETWORK
	for file in $(NETWORK_SOURCE); do cat $$file >>NETWORK; echo >>NETWORK; done

ENVIRON: $(ENVIRON_SOURCE)
	rm -f ENVIRON
	for file in $(ENVIRON_SOURCE); do cat $$file >>ENVIRON; echo >>ENVIRON; done

WEBREPORT: $(WEBREPORT_SOURCE)
	rm -f WEBREPORT
	for file in $(WEBREPORT_SOURCE); do cat $$file >>WEBREPORT; echo >>WEBREPORT; done

include $(BUILDRULES)

default_pcp : default

install_pcp : install
