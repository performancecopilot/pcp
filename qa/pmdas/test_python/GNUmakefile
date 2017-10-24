#
# Copyright (c) 2014 Aconex
# Copyright (c) 2014,2017 Red Hat
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

IAM	= test_python
TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/$(IAM)
PYSCRIPT = pmda$(IAM).python

LDIRT = domain.h root pmns $(IAM).log

default default_pcp setup:

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 Install Remove $(TESTDIR)
	$(INSTALL) -m 644 $(PYSCRIPT) $(TESTDIR)/$(PYSCRIPT)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
