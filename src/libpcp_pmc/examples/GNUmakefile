#!gmake
#

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

CXXFILES	= fixed.c++ hotproc.c++
TARGETS		= fixed hotproc
LCXXDEFS	= $(DEBUG)
LCXXFLAGS	+= -I..
LLDLIBS		= -L../src -lpcp_pmc -lpcp $(LIB_FOR_BASENAME)
LDIRT		= $(TARGETS)

default:	$(TARGETS)

include $(BUILDRULES)

install:	$(TARGETS)

fixed:	fixed.o
	rm -f $@
	$(CXXF) -o $@ fixed.o $(LDFLAGS) $(LDLIBS)

hotproc:	hotproc.o
	rm -f $@
	$(CXXF) -o $@ hotproc.o $(LDFLAGS) $(LDLIBS) -lncurses

default_pcp : default

install_pcp : install
