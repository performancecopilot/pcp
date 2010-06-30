#!gmake
#
# Copyright (c) 2010 Aconex.  All Rights Reserved.
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

IMAGES = GliderBanner.bmp GliderDialog.bmp
CONFIGS = pcp.fstab pcp.profile
BATFILES = pcpsh.bat postinst.bat prerm.bat setup.bat Glider.bat
PERLFILES = Glider.pm LaunchExe.pm LaunchScript.pm
LSRCFILES = $(IMAGES) $(BATFILES) $(PERLFILES) $(CONFIGS) README WiX.patch

default: 

include $(BUILDRULES)

install: 

default_pcp : default

install_pcp : install
