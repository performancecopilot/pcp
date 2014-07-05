#!gmake
#
# Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
include $(TOPDIR)/src/include/builddefs.pro

CXXFILES = pmview.c++ ColorList.c++ \
	   error.c++ BarMod.c++ BarObj.c++ BaseObj.c++ \
	   DefaultObj.c++ GridObj.c++ LabelObj.c++ StackObj.c++ \
	   Text.c++ ViewObj.c++ PipeObj.c++ Link.c++ Xing.c++ \
	   SceneFileObj.c++

GEN_CXXFILES = theGram.c++ theLex.c++ 
TARGETS   = pmview noview

LCXXFLAGS = '-DPCP_VERSION="$(PCP_VERSION)"' $(PCP_OMCINV_INC_PATH) \
	    $(PCP_INC_PATH) $(DEBUG) $(PCP_X11_CFLAGS) $(PCP_ISV_CFLAGS)

LLDFLAGS  = $(PCP_LIBS_PATH) -L$(TOPDIR)/src/libpcp_mon/src \
	    -L$(TOPDIR)/src/libpcp/src $(PCP_X11_LDFLAGS) $(PCP_ISV_LDFLAGS)

LLDLIBS   = $(PCP_OMCINV_STATIC_LIBS) -lpcp_mon -lpcp \
	    -lInventorXt -lInventor -ljpeg -lFL -lfreetype -limage \
	    $(OGL_LIBS) -lvk $(MOTIF_STATIC_LIB) -lXmu \
	    -lXpm -lICE -lXt -lXp -lXi -lX11 -lm 

LSRCFILES = noview app-defaults pmview_static.gz
LDIRT     = pmview.inv ii_files $(GEN_CXXFILES) gram.h y.tab.? PmView+ \
	    theGram.o theLex.o pmview pmview_static

CC	  = $(CXX)

default: $(CXXFILES) $(TARGETS)

include $(TOPDIR)/src/include/buildrules.pro

install: $(TARGETS) PmView+ 
	$(INSTALL) -m 755 pmview $(PCP_BIN_DIR)/pmview+
	$(INSTALL) -m 644 PmView+ $(PCP_X11_APPDEFS_DIR)/PmView+ 
	$(INSTALL) -m 755 noview $(PCP_BIN_DIR)/oview

theLex.c++ : lex.l
	$(LEX) -t lex.l > $@

gram.h y.tab.c : gram.y
	$(YACC) -d gram.y && cp y.tab.h gram.h

theGram.c++ : y.tab.c
	cp y.tab.c $@

theLex.o : gram.h

default_pro : default

install_pro : install

PmView+ : app-defaults
	$(CPP_SIMPLE) -P -D__linux $< > $@

pmview : $(OBJECTS) theGram.o theLex.o ../libpcp_inv/src/libpcp_inv.a
	$(CXXF) -o $@ $(OBJECTS) theLex.o theGram.o $(LLDFLAGS) $(LLDLIBS)
