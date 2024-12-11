TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

ifeq "$(PMDA_BPF)" "true"
CFILES	= bpf.c
CMDTARGET = pmdabpf$(EXECSUFFIX)
LIBTARGET = pmda_bpf.$(DSOSUFFIX)
LLDLIBS = $(PCP_WEBLIB) $(LIB_FOR_LIBBPF) $(LIB_FOR_LIBELF) -lz -lm -ldl
ifeq "$(HAVE_LIBINIH)" "true"
LLDLIBS += $(LIB_FOR_INIH)
endif
LCFLAGS = -I.
CONFIG	= bpf.conf
DFILES	= README
SUBDIRS	= modules

IAM	= bpf
DOMAIN	= BPF

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX)

default_pcp default:	$(CMDTARGET) $(LIBTARGET) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install_pcp install:	default $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(CMDTARGET) $(LIBTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/domain.h domain.h $(PMDAADMDIR)/domain.h
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/root_bpf root_bpf $(PMDAADMDIR)/root_bpf
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_bpf root_bpf $(PCP_PMNSADM_DIR)/root_bpf
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)
	@$(INSTALL_MAN)
	$(SUBDIRS_MAKERULE)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmns:
	$(LN_S) -f root_bpf pmns
else
default_pcp default:

install_pcp install:
	@$(INSTALL_MAN)
endif

check:: $(MAN_PAGES)
	$(MANLINT) $^

clean::
	$(MAKE) -C modules/ clean
	rm -f $(LDIRT)
debug:
	@echo PMDA_BPF=$(PMDA_BPF)
