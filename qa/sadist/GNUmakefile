#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/sadist

SCRIPT = mksa-sysstat
SADBIN = $(shell echo sa-sysstat-*.xz) \
	 csb6-rhel6.bin.xz 859117-rhel5.bin.xz f16-sa20.bin.xz \
	 rhel5-sa20.bin.xz
SADXML = csb6-rhel6.xml.xz smash-rhel6.xml.xz smash-rhel5.xml.xz \
	 859117-rhel5.xml.xz 891688-dash-time.xml
IOSTAT = iostat iostat-c iostat-d iostat-d-k iostat-d-m iostat-p-ALL \
	 iostat-t iostat-t.ISO iostat-x iostat-z iostat-z-p-ALL \
	 iostat-t-x-bud

# override ${MANDIRT} from buildefs, we want to keep e.g. *.[1-9].xz
MANDIRT =

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPT) $(TESTDIR)/$(SCRIPT)
	$(INSTALL) -m 644 $(SADBIN) $(SADXML) $(IOSTAT) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
