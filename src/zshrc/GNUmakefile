#
# Copyright (c) 2016 Red Hat.
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

ZSHRC = _pcp
ZSHDIR = $(PCP_SHARE_DIR)/../zsh/site-functions

default:	$(ZSHRC)

include $(BUILDRULES)

install:	default
	$(INSTALL) -d $(ZSHDIR)
	$(INSTALL) -m 644 $(ZSHRC) $(ZSHDIR)/$(ZSHRC)

default_pcp:	default

install_pcp:	install
