TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM	= bpf
LDLIBS = $(PCP_WEBLIB) -lbpf -lelf -lz -lm -ldl
MODULETMP = $(PCP_PMDAS_DIR)/$(IAM)/modules
MODULEDIR = $(PCP_PMDASADM_DIR)/$(IAM)/modules

MODULES	= \
	biolatency.so \
	runqlat.so    \
	execsnoop.so  \

COMMON_OBJ = \
	trace_helpers.o   \
	syscall_helpers.o \
	errno_helpers.o   \
	map_helpers.o     \
	uprobe_helpers.o  \


# ported helper files.
HELPERS_H = \
	btf_helpers.h     \
	errno_helpers.h   \
	map_helpers.h     \
	syscall_helpers.h \
	trace_helpers.h   \
	uprobe_helpers.h  \

HELPERS_C = \
	btf_helpers.c     \
	errno_helpers.c   \
	map_helpers.c     \
	syscall_helpers.c \
	trace_helpers.c   \
	uprobe_helpers.c  \

# ported header file.
APPS_H = \
	bashreadline.h   \
	bindsnoop.h      \
	biopattern.h     \
	biosnoop.h       \
	biostacks.h      \
	bitesize.h       \
	cpudist.h        \
	cpufreq.h        \
	drsnoop.h        \
	execsnoop.h      \
	exitsnoop.h      \
	filelife.h       \
	filetop.h        \
	fsdist.h         \
	fsslower.h       \
	funclatency.h    \
	gethostlatency.h \
	hardirqs.h       \
	klockstat.h      \
	ksnoop.h         \
	llcstat.h        \
	mountsnoop.h     \
	offcputime.h     \
	oomkill.h        \
	opensnoop.h      \
	readahead.h      \
	runqlen.h        \
	runqslower.h     \
	softirqs.h       \
	solisten.h       \
	statsnoop.h      \
	syscount.h       \
	tcpconnect.h     \
	tcpconnlat.h     \
	tcprtt.h         \
	tcpsynbl.h       \
	vfsstat.h        \

# ported *.bpf.c files.
APPS_BPF = \
	bashreadline.bpf.c   \
	bindsnoop.bpf.c      \
	biopattern.bpf.c     \
	biosnoop.bpf.c       \
	biostacks.bpf.c      \
	bitesize.bpf.c       \
	cachestat.bpf.c      \
	cpudist.bpf.c        \
	cpufreq.bpf.c        \
	drsnoop.bpf.c        \
	execsnoop.bpf.c      \
	exitsnoop.bpf.c      \
	filelife.bpf.c       \
	filetop.bpf.c        \
	fsdist.bpf.c         \
	fsslower.bpf.c       \
	funclatency.bpf.c    \
	gethostlatency.bpf.c \
	hardirqs.bpf.c       \
	klockstat.bpf.c      \
	ksnoop.bpf.c         \
	llcstat.bpf.c        \
	mountsnoop.bpf.c     \
	numamove.bpf.c       \
	offcputime.bpf.c     \
	oomkill.bpf.c        \
	opensnoop.bpf.c      \
	readahead.bpf.c      \
	runqlen.bpf.c        \
	runqslower.bpf.c     \
	softirqs.bpf.c       \
	solisten.bpf.c       \
	statsnoop.bpf.c      \
	syscount.bpf.c       \
	tcpconnect.bpf.c     \
	tcpconnlat.bpf.c     \
	tcprtt.bpf.c         \
	tcpsynbl.bpf.c       \
	vfsstat.bpf.c        \

# ported vmlinux.h files
VMLINUX = \
	x86     \
	powerpc \
	arm64   \


default_pcp default:	$(HELPERS_H) $(HELPERS_C) $(APPS_H) $(APPS_BPF) \
	$(VMLINUX) $(COMMON_OBJ) $(MODULES)

include $(BUILDRULES)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(MODULEDIR)
	$(INSTALL) -m 755 -d $(MODULETMP)
	$(INSTALL) -m 644 -t $(MODULETMP) $(MODULES) $(MODULEDIR)


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

$(VMLINUX):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/iovisor/bcc/libbpf-tools/$@ $@

LDIRT = $(HELPERS_H) $(HELPERS_C) $(APPS_H) $(APPS_BPF) $(VMLINUX)

%.bpf.o: %.bpf.c
	$(PMDABPF_CLANG) -Wall -g -O2 -target bpf -D__TARGET_ARCH_$(PMDABPF_ARCH) \
		-I./$(PMDABPF_ARCH)/ -c $< -o $@
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
	rm -f $(MODULES) *.o *.skel.h
