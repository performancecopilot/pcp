TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

default_pcp default:
ifeq "$(PMDABPF_BPFTOOL)" "$(abspath github.com/libbpf/bpftool/src/bpftool)"
	make -C github.com/libbpf/bpftool/src
endif

install_pcp install:

check:

setup:

# we need a specific clean step for some vendored code
clean::
	make -C github.com/libbpf/bpftool/src clean
