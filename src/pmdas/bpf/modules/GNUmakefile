TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM	= bpf
LDLIBS = $(PCP_WEBLIB) -lbpf -lelf -lz -lm -ldl
LDIRT = $(HELPERS_H) $(HELPERS_C) $(APPS_H) $(APPS_BPF) vmlinux.h
MODULETMP = $(PCP_PMDAS_DIR)/$(IAM)/modules
MODULEDIR = $(PCP_PMDASADM_DIR)/$(IAM)/modules

COMMON_OBJ = \
	compat.o \
	trace_helpers.o \
	btf_helpers.o \
	uprobe_helpers.o \
	map_helpers.o \

# ported helper files.
HELPERS_H = \
	compat.h \
	compat.bpf.h \
	bits.bpf.h \
	blk_types.h \
	maps.bpf.h \
	core_fixes.bpf.h \
	btf_helpers.h \
	map_helpers.h \
	trace_helpers.h \
	uprobe_helpers.h \

HELPERS_C = \
	compat.c \
	btf_helpers.c \
	map_helpers.c \
	trace_helpers.c \
	uprobe_helpers.c \

# ported header file.
APPS_H = \
	bashreadline.h \
	biosnoop.h \
	execsnoop.h \
	exitsnoop.h \
	fsslower.h \
	mountsnoop.h \
	oomkill.h \
	opensnoop.h \
	statsnoop.h \
	tcpconnect.h \
	tcpconnlat.h \
	vfsstat.h \

# ported *.bpf.c files.
APPS_BPF = \
	bashreadline.bpf.c \
	biosnoop.bpf.c \
	execsnoop.bpf.c \
	exitsnoop.bpf.c \
	fsslower.bpf.c \
	mountsnoop.bpf.c \
	oomkill.bpf.c \
	opensnoop.bpf.c \
	statsnoop.bpf.c \
	tcpconnect.bpf.c \
	tcpconnlat.bpf.c \
	vfsstat.bpf.c \


default_pcp default:	$(PMDABPF_MODULES)

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

%.bpf.o: %.bpf.c vmlinux.h $(APPS_H) $(APPS_BPF)
	$(CLANG) -Wall -g -O2 -target bpf -D__TARGET_ARCH_$(PMDABPF_ARCH) \
		-I. -c $< -o $@
	$(LLVM_STRIP) -g $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

%.o: %.c
%.o: %.c %.skel.h $(HELPERS_H) $(APPS_H)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

%_helpers.o: %_helpers.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

compat.o: compat.c
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

# most tools require $(COMMON_OBJ), which requires libbpf 0.7+
# explicitly list tools which don't need them and also work with libbpf 0.4
biolatency.so: biolatency.o
	$(LINK.c) -shared $^ -o $@ $(LDLIBS)
runqlat.so: runqlat.o
	$(LINK.c) -shared $^ -o $@ $(LDLIBS)

clean::
	rm -f $(PMDABPF_MODULES) *.o *.skel.h
