#
# Copyright (c) 2014 Aconex
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

TESTS =	ActiveMQTest.pl QueueTest.pl RESTClientTest.pl \
	JVMGarbageCollectionTest.pl JVMMemoryPoolTest.pl JVMMemoryTest.pl \
	CacheTest.pl
MODULES = ActiveMQ.pm Queue.pm RESTClient.pm \
	JVMGarbageCollection.pm JVMMemory.pm JVMMemoryPool.pm \
	Cache.pm TimeSource.pm

IAM	= activemq
DOMAIN	= ACTIVEMQ

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LDIRT = domain.h root pmns *.log

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_ACTIVEMQ)" "true"
build-me: check_domain
install install_pcp: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/pmda$(IAM).pl pmda$(IAM).pl $(PMDAADMDIR)/pmda$(IAM).pl
	$(INSTALL) -m 755 -d $(PMDAADMDIR)/PCP
	$(INSTALL) -m 755 -d $(PMDATMPDIR)/PCP
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/PCP $(MODULES) $(PMDAADMDIR)/PCP
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)

ifeq "$(PMDA_ACTIVEMQ)" "true"
test:
	for test in $(TESTS); do perl $$test || exit 1; done
endif

check :: $(MAN_PAGES)
	$(MANLINT) $^
