TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

default_pcp default:
ifeq "$(PMDA_BPF)" "true"
	make -C github.com/libbpf/bpftool/src
endif

install_pcp install:

check:

setup:

# we need a specific clean step for some vendored code
clean::
	make -C github.com/libbpf/bpftool/src clean
