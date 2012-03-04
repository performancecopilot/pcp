#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include /etc/pcp.conf
endif
ifeq ($(shell [ -d /usr/gnu/bin ] && echo 0),0)
PATH	= /usr/gun/bin:$(shell . /etc/pcp.env; echo $$PATH)
else
PATH	= $(shell . /etc/pcp.env; echo $$PATH)
endif
include $(PCP_INC_DIR)/builddefs

SUBDIRS = src-oss pmdas

TESTS	= $(shell sed -n -e '/^[0-9]/s/[ 	].*//p' <group)

default:	localconfig new remake check qa_hosts $(OTHERS)

install:

default_pcp default_pro:

install_pcp install_pro:

exports install:

clobber cleanup:	$(SUBDIRS)
	rm -rf 051.work
	rm -f *.bak *.bad *.core *.full *.raw *.o core a.out core.*
	rm -f *.log eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz
	rm -f *.full.ok *.new rc_cron_check.clean
	rm -f make.out qa_hosts localconfig localconfig.h check.time
	if [ -d src ]; then cd src; $(MAKE) clobber; else exit 0; fi
	if [ -d src-oss ]; then cd src-oss; $(MAKE) clobber; else exit 0; fi
	find ???.out ????.out -type f -links +1 | xargs rm -f
	rm -f 134.full.*
	# these ones are links to the real files created when the associated
	# test is run
	#
	grep '\.out$$' .gitignore | xargs rm -f
	# from QA 441
	#
	rm -f big1.*
	$(SUBDIRS_MAKERULE)

# 051 depends on this rule being here
051.work/die.001: 051.setup
	chmod u+x 051.setup
	./051.setup

qa_hosts:	qa_hosts.master mk.qa_hosts
	PATH=$(PATH); ./mk.qa_hosts

setup:

localconfig:
	PATH=$(PATH); ./mk.localconfig
