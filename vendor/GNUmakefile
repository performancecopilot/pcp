TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

VERBOSE=V=1

default_pcp default:
ifeq "$(PMDA_BPF)" "true"
	$(MAKE) -C github.com/libbpf/bpftool/src VMLINUX_H=$(PMDABPF_VMLINUXH) $(VERBOSE)
endif

install_pcp install:

check:

setup:

# we need a specific clean step for some vendored code
clean::
	$(MAKE) -C github.com/libbpf/bpftool/src $(VERBOSE) $@
