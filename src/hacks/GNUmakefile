#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LLDLIBS = $(PCPLIB)
EXEC = delta-indom-stats pmresult-fiddle
LDIRT = $(EXEC)
LCFLAGS = -I../libpcp/src

default: $(EXEC)

include $(BUILDRULES)

# just to be sure ...
#
default_pcp:
install:
install_pcp:
check::

delta-indom-stats:	delta-indom-stats.c

pmresult-fiddle:	pmresult-fiddle.c
