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
BASHDIR = $(shell echo $(PCP_SHARE_DIR) | sed -e 's;/pcp$$;;')/bash-completion
COMMANDS = $(shell sed -n -e 's/^complete .*-o default //p' < $(BASHRC))

default:	$(BASHRC)

include $(BUILDRULES)

install:	default
	$(INSTALL) -d $(BASHDIR)
	$(INSTALL) -d $(BASHDIR)/completions
	$(INSTALL) -m 644 $(BASHRC) $(BASHDIR)/completions/pcp
	for c in `echo $(COMMANDS)`; do \
	    [ $$c = pcp ] && continue; \
	    $(INSTALL) -S $(BASHDIR)/completions/pcp $(BASHDIR)/completions/$$c; \
	done

default_pcp:	default

install_pcp:	install
