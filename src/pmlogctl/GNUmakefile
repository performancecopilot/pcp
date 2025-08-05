#
# Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
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

LDIRT = pmie-pmfind.class

default : pmie-pmfind.class

include $(BUILDRULES)

install : default
	$(INSTALL) -m 755 pmlogctl.sh $(PCP_BIN_DIR)/pmlogctl$(SHELLSUFFIX)
	# not installed as a symlink so that selinux file context applies
	$(INSTALL) -m 755 pmlogctl.sh $(PCP_BIN_DIR)/pmiectl$(SHELLSUFFIX)
	$(INSTALL) -m 755 -d $(PCP_ETC_DIR)/pcp/pmlogger/class.d
	$(INSTALL) -m 644 pmlogger-pmfind.class $(PCP_ETC_DIR)/pcp/pmlogger/class.d/pmfind
	$(INSTALL) -m 644 logpush.class $(PCP_ETC_DIR)/pcp/pmlogger/class.d/logpush
	$(INSTALL) -m 755 -d $(PCP_ETC_DIR)/pcp/pmie/class.d
	$(INSTALL) -m 644 pmie-pmfind.class $(PCP_ETC_DIR)/pcp/pmie/class.d/pmfind

default_pcp : default

install_pcp : install

pmie-pmfind.class: pmlogger-pmfind.class
	$(SED) -e 's/pmlogctl/pmiectl/g' -e '/PCP_ARCHIVE_DIR\/[^ ]*/s//PCP_LOG_DIR\/pmie\/%i\/pmie.log/' <pmlogger-pmfind.class >pmie-pmfind.class
