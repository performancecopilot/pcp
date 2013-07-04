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
include localdefs

WORKDIR	:= $(shell pwd)
GROUP	:= $(shell basename $(WORKDIR))
GROUPDIR	= $(PCP_VAR_DIR)/config/pmlogconf

LDIRT	= GNUmakefile

DEFAULT	= $(FILES)

LSRCFILES = localdefs $(DEFAULT)

CONFIGS	= $(subst "./","",$(DEFAULT))

default_pcp: $(DEFAULT)

install_pcp: install

install: default_pcp
	$(INSTALL) -d $(GROUPDIR)/$(GROUP)
	@for f in $(CONFIGS); do \
	    $(INSTALL) -m 644 $$f $(GROUPDIR)/$(GROUP)/$$f; \
	done

include $(BUILDRULES)
