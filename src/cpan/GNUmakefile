#!gmake
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

PERLMOD   = PMDA.pm
INTERFACE = PMDA.xs local.h typemap
TESTCODE  = test.pl cvalue.c
PERLDOCS  = Changes MANIFEST TODO
LSRCFILES = Makefile.PL $(PERLDOCS) $(PERLMOD) $(INTERFACE) $(TESTCODE)

LDIRT = Makefile COPYING PMDA.bs PMDA.c PMDA.o cvalue pm_to_blib

default: PMDA.o

PMDA.o: Makefile PMDA.xs
	$(MAKE) -f Makefile

Makefile: COPYING
	perl Makefile.PL

COPYING:
	$(LN_S) $(TOPDIR)/COPYING COPYING

test:
	$(MAKE) -f Makefile test

include $(BUILDRULES)

install: default
	#$(MAKE) -f Makefile install

default_pcp: default

install_pcp: install

