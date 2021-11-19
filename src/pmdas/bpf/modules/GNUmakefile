TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM	= bpf
MODULES	= biolatency.so runqlat.so
MODULETMP = $(PCP_PMDAS_DIR)/$(IAM)/modules
MODULEDIR = $(PCP_PMDASADM_DIR)/$(IAM)/modules

default_pcp default:	$(MODULES)

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

vmlinux.h: $(PMDABPF_VMLINUXH)
	ln -s $< $@

%.bpf.o: %.bpf.c vmlinux.h
	$(PMDABPF_CLANG) -Wall -g -O2 -target bpf -D__TARGET_ARCH_$(PMDABPF_ARCH) -c $< -o $@
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

%.so: %.o
	$(LINK.c) -lbpf -shared $^ -o $@

clean::
	rm -f $(MODULES) *.o *.skel.h vmlinux.h
