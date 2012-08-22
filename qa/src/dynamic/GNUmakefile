TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

MYTESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/src/dynamic
MYSCRIPTS = Install Remove
PMDAFILES = domain.h pmns help dynamic.c
LSRCFILES = $(MYSCRIPTS) $(PMDAFILES) GNUmakefile.install

default:

install:
	$(INSTALL) -m 755 -d $(MYTESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(MYTESTDIR)
	$(INSTALL) -m 644 $(PMDAFILES) $(MYTESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(MYTESTDIR)/GNUmakefile

include $(BUILDRULES)
