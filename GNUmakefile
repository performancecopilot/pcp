#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# $Id: GNUmakefile,v 1.10 2005/07/10 23:46:11 kenmcd Exp $
#

-include ./GNUlocaldefs

SUBDIRS = home

SHELL	= /bin/sh

PATH	= $(shell PATH=/sbin:/bin:/usr/sbin:/usr/bin . /etc/pcp.env; echo $$PATH)
MAKEOPTS = --no-print-directory

TESTS	= $(shell sed -e 's/ .*//' owner)

OTHERS	=

default:	new remake check qa_hosts $(OTHERS)

install:

default_pcp default_pro:

install_pcp install_pro:

src-pcp:

exports install:

clobber cleanup:
	rm -rf 051.work
	rm -f *.bak *.bad *.core *.full *.raw *.o core a.out core.*
	rm -f *.log eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz
	rm -f *.full.ok *.new rc_cron_check.clean
	rm -f sudo make.out qa_hosts localconfig localconfig.h check.time
	if [ -d src ]; then cd src; $(MAKE) clobber; else exit 0; fi
	if [ -d src-oss ]; then cd src-oss; $(MAKE) clobber; else exit 0; fi
	find ???.out ????.out -type f -links +1 | xargs rm -f
	rm -f 605.out
	rm -f 134.full.*

# 051 depends on this rule being here
051.work/die.001: 051.setup
	chmod u+x 051.setup
	./051.setup

sudo:	sudo.c
	cc -o sudo sudo.c
	chown root sudo
	chmod 4755 sudo
	@echo "NOTE sudo is a giant security hole, but this is needed by the PCP QA"

qa_hosts:	qa_hosts.master mk.qa_hosts
	PATH=$(PATH); ./mk.qa_hosts

setup:

PLATFORM=0
ifeq ($(PCP_PLATFORM), irix)
PLATFORM=1
endif
ifeq ($(PCP_PLATFORM), linux)
PLATFORM=2
endif

localconfig:
	PATH=$(PATH); ./mk.localconfig
