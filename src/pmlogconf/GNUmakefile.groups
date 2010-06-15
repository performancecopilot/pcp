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
