#!smake -ki
#
# Top level QA makefile for IRIX
#
# $Id: Makefile,v 2.191 2004/06/24 06:15:36 kenmcd Exp $
#

TESTS	!= sed -e '/^\#/d' -e 's/ .*//' owner

OTHERS = 051.work/die.001

LDIRT	= 

default:	new remake check qa_tests qa_hosts $(OTHERS)

qa_tests:
	@$(MAKE) $(MAKEFLAGS) $(TESTS) | grep -v 'is up to date'

exports install:

clobber clean:
	rm -rf 051.work $(LDIRT)
	rm -f *.bak *.bad *.core *.full *.raw *.o core a.out 
	rm -f *.log eek* urk* so_locations
	rm -f sudo make.out qa_hosts localconfig localconfig.h check.time
	if [ -d src ]; then cd src; make clobber; else exit 0; fi
	if [ -d src-oss ]; then cd src-oss; make clobber; else exit 0; fi
	if [ -d libirixpmda ]; then cd libirixpmda; make clobber; else exit 0; fi

# 051 depends on this rule being here
051.work/die.001: 051.setup
	./sudo chown `id -u` 051.setup
	chmod u+x 051.setup
	./051.setup

sudo:	sudo.c
	unset TOOLROOT ROOT; cc -o sudo sudo.c
	chown root.ptg sudo
	chmod 4750 sudo

qa_hosts:	qa_hosts.master mk.qa_hosts
	./mk.qa_hosts

setup:

localconfig:
	./mk.localconfig

