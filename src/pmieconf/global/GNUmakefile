#
# Copyright (c) 2002 Silicon Graphics, Inc.
# All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

RFILES	= enln_actions tngfw_actions parameters ov_actions pcp_actions
WORKDIR	:= $(shell pwd)
GROUP	:= $(shell basename $(WORKDIR))
RULES	= $(PCP_VAR_DIR)/config/pmieconf

LSRCFILES = $(RFILES)
DEFAULT	= $(RFILES)

CONFIGS	= $(subst "./","",$(DEFAULT))

default_pcp: $(DEFAULT)

install_pcp: install

install: default_pcp
	$(INSTALL) -d $(RULES)/$(GROUP)
	@for f in $(CONFIGS); do \
	    $(INSTALL) -m 644 ../rules/$(GROUP)/$$f $(RULES)/$(GROUP)/$$f; \
	done

rules:
	@test ! -d ../rules/$(GROUP) && mkdir ../rules/$(GROUP); exit 0
	@rm -f ../rules/$(GROUP)/*
	@for f in `echo $(DEFAULT)`; do \
	    sed -e's|/usr/pcp/lib/pmie_email|$(PCP_BINADM_DIR)/pmie_email|' \
		-e's|/usr/pcp/bin/pmpost|$(PCP_BINADM_DIR)/pmpost|'  $$f \
		 > ../rules/$(GROUP)/$$f; \
	done

pmlogger.config:
	@if [ -n "$(CONFIGS)" ]; then /bin/sh ../xtractnames $(CONFIGS); fi

include $(BUILDRULES)
