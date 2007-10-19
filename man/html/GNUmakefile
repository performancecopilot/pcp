#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write the Free Software Foundation,
# Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTMLPAGES = $(shell echo *.html)
CONFIGURE = kmchart.adp.in kmtime.adp.in
ASSISTANT = kmchart.adp kmtime.adp
LSRCFILES = $(HTMLPAGES) $(CONFIGURE)
LDIRT = $(ASSISTANT)

default: $(HTMLPAGES) $(ASSISTANT)

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html
	$(INSTALL) -m 644 $(ASSISTANT) $(HTMLPAGES) $(PKG_DOC_DIR)/html
