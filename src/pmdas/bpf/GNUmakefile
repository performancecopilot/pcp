TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CFILES	= bpf.c
CMDTARGET = pmdabpf$(EXECSUFFIX)
LIBTARGET = pmda_bpf.$(DSOSUFFIX)
LLDLIBS = -lbpf $(PCP_WEBLIB)
LLDLIBS += $(LIB_FOR_ZLIB) $(LIB_FOR_MATH) $(LIB_FOR_LIBELF)
ifeq "$(HAVE_LIBINIH)" "true"
LLDLIBS += $(LIB_FOR_INIH)
endif
LLDFLAGS += -fPIC -L./bpf
LCFLAGS = -fPIC -I. -I./bpf/include
CONFIG	= bpf.conf
DFILES	= README
SCRIPTS = Install Remove Upgrade
SUBDIRS	= modules

IAM	= bpf
DOMAIN	= BPF
PMDAINIT = $(IAM)_init

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
LOCALCONF_LINE = "bpf	157	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

VERSION_SCRIPT = exports
LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX) bpf $(VERSION_SCRIPT)

default: build-me

include $(BUILDRULES)

ifeq "$(PMDA_BPF)" "true"
build-me:	$(CMDTARGET) $(LIBTARGET) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install:	default $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(CMDTARGET) $(LIBTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/domain.h domain.h $(PMDAADMDIR)/domain.h
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/root_bpf root_bpf $(PMDAADMDIR)/root_bpf
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_bpf root_bpf $(PMDAADMDIR)/root_bpf
	$(INSTALL) -S $(PMDAADMDIR)/root_bpf $(PCP_PMNSADM_DIR)/root_bpf
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)
	@$(INSTALL_MAN)
	$(SUBDIRS_MAKERULE)
else
build-me:
install:
	@$(INSTALL_MAN)
endif

default_pcp : default

install_pcp : install

$(OBJECTS): domain.h bpf

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

bpf:
	$(LN_S) -f $(TOPDIR)/vendor/github.com/libbpf/bpftool/src/bootstrap/libbpf bpf

pmns:
	$(LN_S) -f root_bpf pmns

check:: $(MAN_PAGES)
	$(MANLINT) $^

clean::
	$(MAKE) -C modules/ clean
	rm -f $(LDIRT)

debug:
	@echo PMDA_BPF=$(PMDA_BPF)
