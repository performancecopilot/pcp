#
# Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#
# countd pmimport sample plugin
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

LIBTARGET = countd.$(DSOSUFFIX)

DOMAIN  = COUNTD
STDPMID = $(TOPDIR)/src/pmns/stdpmid
LDIRT = domain.h

default: $(LIBTARGET) countd.c domain.h

include $(BUILDRULES)

domain.h: $(STDPMID)
	rm -f domain.h
	echo "/*" >domain.h
	echo " * Built from $(STDPMID)" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) < $(STDPMID) >> domain.h '/#define[  ][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

install: default
	# $(INSTALL) -m 755 $(DEMO_DIR)
	# $(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	# $(INSTALL) -m 644 countd.c countproc $(DEMO_DIR)
	# $(INSTALL) -m 644 GNUmakefile.install $(DEMO_DIR)/GNUmakefile

default_pcp:

install_pcp:
