#! gmake
#
# Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
# 


TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs.pro

CXXFILES= \
    App.c++ ColScaleMod.c++ ColorMod.c++ ColorScale.c++ Form.c++ FormUI.c++ \
    Inv.c++ Launch.c++ LaunchMenu.c++ Menu.c++ MetricList.c++ ModList.c++ \
    Modulate.c++ Record.c++ ScaleMod.c++ StackMod.c++ ToggleMod.c++ View.c++ \
    Window.c++ YScaleMod.c++ tv.c++ PCPColor.c++

HFILES = \
    App.h ColScaleMod.h ColorMod.h ColorScale.h Form.h FormUI.h Inv.h \
    Launch.h LaunchMenu.h Menu.h MetricList.h ModList.h Modulate.h \
    Record.h ScaleMod.h StackMod.h ToggleMod.h View.h Window.h YScaleMod.h \
    tv.h PCPColor.h

# Note:
# add -DMENU_FILE_PRINT to LCXXFLAGS if you want the File/Print menu item
# but it's really of no use (and totally busted on linux).

LCXXFLAGS = -include /usr/include/math.h \
	   $(PCP_OMCINV_INC_PATH) $(MOTIF_INC_PATH) \
	   $(PCP_X11_CFLAGS) $(PCP_ISV_CFLAGS)

LDIRT = tv.c++ tv.h libpcp_inv.a

#
# The default target, 'all', depends on the building of the target
# executable(s).
#
default: libpcp_inv.a

include $(TOPDIR)/src/include/buildrules.pro

#
# The executable depends on the object files. An implicit rule
# builds the object files from the source files.
#
libpcp_inv.a : $(OBJECTS)
	$(AR) crv libpcp_inv.a $(OBJECTS)
	ranlib libpcp_inv.a

install: default

View.o tv.o: tv.h

tv.h tv.c++:
	@rm -f $@ && ln -s $(TOPDIR)/src/pmtime/common/$@ $@


default_pro : default
