#!smake
#
PCP_SRC_DEPTH = ../../src
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmtop.c
TARGETS	= pmtop
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDLIBS	= $(PCPLIB)
LDIRT	=

default:	$(TARGETS)

include $(COMMONRULES)

install:	$(TARGETS)
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmtop

pmtop:	$(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D14 $@
