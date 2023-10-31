#
# Copyright (c) 2022-2023 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pcp-geolocate
SCRIPT = $(TARGET).py
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
LDIRT = $(TARGET).service

default: $(SCRIPT) $(MAN_PAGES) $(TARGET).service

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BINADM_DIR)/$(TARGET)
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 $(TARGET).service $(PCP_SYSTEMDUNIT_DIR)/$(TARGET).service
endif
	@$(INSTALL_MAN)
endif

default_pcp : default

install_pcp : install

$(TARGET).service: $(TARGET).service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	    -e 's;@PCP_SYSCONF_DIR@;'$(PCP_SYSCONF_DIR)';' \
	# END

check :: $(SCRIPT)
	$(PYLINT) $^

check :: $(MAN_PAGES)
	$(MANLINT) $^
