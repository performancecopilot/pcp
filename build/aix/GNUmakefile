#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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

TEMPLATE = pcp.template
LDIRT += $(TEMPLATE) $(TEMPLATE).i
LSRCFILES = $(TEMPLATE).in

default install install_pcp default_pcp :

pack_pcp : $(TEMPLATE)
	$(MKINSTALLP) -d $${DIST_ROOT} -T $(TEMPLATE)

$(TEMPLATE): $(TEMPLATE).in
	@echo Generating $@ from $?; \
	sed -e's|@package_name@|$(PACKAGE_NAME)|g' \
	    -e's|@package_version@|$(PACKAGE_VERSION)|g' \
	    -e's|@package_release@|$(PACKAGE_BUILD)|g' < $? > $(TEMPLATE).i ; \
	@find $${DIST_ROOT} -print < $(TEMPLATE).i | \
	    sed -e "s|^$${DIST_ROOT}|    |g" \ >> $@ ; \
	@echo "  EOFiles" >> $@ ; \
	@echo "EOFileset" >> $@

include $(BUILDRULES)

