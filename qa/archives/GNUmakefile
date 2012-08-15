TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/archives
SRCARCH = chartqa1.0 viewqa1.0 viewqa2.0 viewqa3.0 rattle.0 oview-short.0 \
	  snort-disks.0 vldb-disks.0 moomba.pmkstat.0 fcsw_indom.0
# make substitution/expansion does funky things with these filenames
DATEARCH = 20071023.08\:06\:36.0 20071023.08\:06\:36.index 20071023.08\:06\:36.meta
SRCEXTRAS = $(subst 0,meta,$(SRCARCH)) $(subst 0,index,$(SRCARCH))
LSRCFILES = $(SRCARCH) $(SRCEXTRAS) $(DATEARCH) GNUmakefile.install

MKARCH = gap.0 gap2.0
MKEXTRAS = $(subst 0,meta,$(MKARCH)) $(subst 0,index,$(MKARCH))
LDIRT = $(MKARCH) $(MKEXTRAS) pmlogger.log
LDIRDIRT = tmp

default:

install:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 755 $(SRCARCH) $(SRCEXTRAS) $(DATEARCH) $(TESTDIR)

setup: default $(MKARCH)

include $(BUILDRULES)

# gap: 10 sec data, 5 sec gap, 10 sec data
# gap2: 10 sec data, 5 sec gap, 10 sec data, 5 sec gap, 10 sec data
# sample.bin instances change across gaps
gap.0 gap2.0:
	$(MAKE) -f GNUmakefile.install setup
