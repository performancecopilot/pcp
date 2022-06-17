TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM	= bpf
LDLIBS = $(PCP_WEBLIB) -lbpf -lelf -lz -lm -ldl
MODULETMP = $(PCP_PMDAS_DIR)/$(IAM)/modules
MODULEDIR = $(PCP_PMDASADM_DIR)/$(IAM)/modules

COMMON_OBJ = \
	trace_helpers.o \
	btf_helpers.o \
	uprobe_helpers.o \

# ported helper files.
HELPERS_H = \
	trace_helpers.h \
	btf_helpers.h \
	uprobe_helpers.h \

HELPERS_C = \
	trace_helpers.c \
	btf_helpers.c \
	uprobe_helpers.c \

# ported header file.
APPS_H = \
	bashreadline.h \
	execsnoop.h \
	exitsnoop.h \
	fsslower.h \
	mountsnoop.h \
	oomkill.h \
	opensnoop.h \
	statsnoop.h \
	tcpconnlat.h \
	vfsstat.h \

# ported *.bpf.c files.
APPS_BPF = \
	bashreadline.bpf.c \
	execsnoop.bpf.c \
	exitsnoop.bpf.c \
	fsslower.bpf.c \
	mountsnoop.bpf.c \
	oomkill.bpf.c \
	opensnoop.bpf.c \
	statsnoop.bpf.c \
	tcpconnlat.bpf.c \
	vfsstat.bpf.c \


default_pcp default:	$(HELPERS_H) $(HELPERS_C) $(APPS_H) $(APPS_BPF) \
	$(COMMON_OBJ) $(PMDABPF_MODULES)

include $(BUILDRULES)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(MODULEDIR)
	$(INSTALL) -m 755 -d $(MODULETMP)
	$(INSTALL) -m 644 -t $(MODULETMP) $(PMDABPF_MODULES) $(MODULEDIR)


# Use the clang pipeline to emit LLVM to LLD and emit BPF straight to an ELF .o.
# The GCC pipeline has parts of this but not completely, and in any case, likely
# requires version/feature detection too.
#
# Some distributions place commands at different names (llvm-dis vs llvm-dis-12),
# so detect distro directly.
#
# clang-12 special notes: https://www.spinics.net/lists/netdev/msg689901.html

$(HELPERS_H):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/iovisor/bcc/libbpf-tools/$@ $@

$(HELPERS_C):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/iovisor/bcc/libbpf-tools/$@ $@

$(APPS_H):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/iovisor/bcc/libbpf-tools/$@ $@

$(APPS_BPF):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/iovisor/bcc/libbpf-tools/$@ $@

vmlinux.h: $(PMDABPF_VMLINUXH)
	$(LN_S) -f $< $@

LDIRT = $(HELPERS_H) $(HELPERS_C) $(APPS_H) $(APPS_BPF) vmlinux.h

%.bpf.o: %.bpf.c vmlinux.h
	$(PMDABPF_CLANG) -Wall -g -O2 -target bpf -D__TARGET_ARCH_$(PMDABPF_ARCH) \
		-I. -c $< -o $@
	$(PMDABPF_LLVM_STRIP) -g $@

%.skel.h: %.bpf.o
	$(PMDABPF_BPFTOOL) gen skeleton $< > $@

%.o: %.c
%.o: %.c %.skel.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

# ifeq "$(CLANG_MAJOR_VERSION)" "12"
# %.bpf.o: %.bpf.c FORCE
# 	clang -emit-llvm -O2 -Wno-unknown-attributes -Xclang -disable-llvm-passes -g -c $< -o - | opt -O2 -mtriple=bpf-pc-linux | llvm-dis | llc -march=bpf -filetype=obj -o $@

# else
# %.bpf.o: %.bpf.c FORCE
# 	# clang -Wall -emit-llvm -O2 -Wno-unknown-attributes -g -c $< -o - | llc -march=bpf -filetype=obj -o $@
# 	clang -Wall -g -O2 -target bpf -D__TARGET_ARCH_x86 -c $< -o $@ && llvm-strip -g $@

# endif

%.so: %.o $(COMMON_OBJ)
	$(LINK.c) -shared $^ -o $@ $(LDLIBS)

%_helpers.o: %_helpers.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean::
	rm -f $(PMDABPF_MODULES) *.o *.skel.h
