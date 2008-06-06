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

default:	build-me

include $(BUILDRULES)

ifeq "$(PMC_SUPPORT)" "true"
build-me:	$(TARGETS)
install:	build-me
else
build-me:
install:
endif

fixed:	fixed.o
	rm -f $@
	$(CXXF) -o $@ fixed.o $(LDFLAGS) $(LDLIBS)

hotproc:	hotproc.o
	rm -f $@
	$(CXXF) -o $@ hotproc.o $(LDFLAGS) $(LDLIBS) -lncurses

default_pcp : default

install_pcp : install
