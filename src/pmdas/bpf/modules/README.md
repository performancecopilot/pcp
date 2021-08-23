BPF modules here.


Generating `vmlinux_cut.h`
==========================

Generally speaking, this header will only need to be regenerated if new type structs
are used from the kernel, that are not currently used in modules. In order to do this,
we run the following commands:

```
$ bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

This generates a complete vmlinux.h, which is 2-3MB in size. Rather than carry around
the full header in the repo, we only carry the pieces we need. So, the easiest way to
do this is to copy the required definitions from `vmlinux.h` into `vmlinux_cut.h`.
Once you have done this, your build using new kernel-side types should succeed.

