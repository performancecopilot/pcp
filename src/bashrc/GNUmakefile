#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

BASHRC = pcp_completion.sh
BASHDIR = $(PCP_ETC_DIR)/bash_completion.d
LSRCFILES = $(BASHRC)

default:	$(BASHRC)

include $(BUILDRULES)

install:	default
	$(INSTALL) -d $(BASHDIR)
	$(INSTALL) -m 644 $(BASHRC) $(BASHDIR)/pcp

default_pcp:	default

install_pcp:	install
