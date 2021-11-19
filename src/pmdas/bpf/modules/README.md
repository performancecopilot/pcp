BPF modules here.

# `vmlinux.h`

`vmlinux.h` contains architecture specific type information from the kernel.
Ideally it is generated during the build, in order to catch any compilation
errors beforehand (e.g. rename of a struct member in the kernel). When the
`vmlinux.h` doesn't match the kernel types, and the eBPF program is accessing
a (for example) renamed struct member, the eBPF verifier will block the program
at runtime.

However, some build environments don't have BTF enabled or are running with an
old kernel, therefore we're vendoring `vmlinux.h` files generated on different
architectures.

If `--with-pmdabpf-btf` is passed to the `./configure` script, `btftool` will
generate a `vmlinux.h` on-the-fly, otherwise we fall back to the vendored
`vmlinux.h` files.

Generally speaking, this header will only need to be regenerated if new type structs
are used from the kernel, that are not currently used in modules. In order to do this,
we run the following commands:

```
$ bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

This generates a complete vmlinux.h, which is 2-3MB in size.
