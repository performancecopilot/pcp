BPF modules here.

# `vmlinux.h`

`vmlinux.h` contains architecture specific type information from the
kernel.  Ideally it is generated during the build, in order to catch
any compilation errors beforehand (e.g. rename of a struct member in
the kernel).  When the `vmlinux.h` doesn't match the kernel types,
and the eBPF program performs a problematic operation (e.g. accessing
a renamed struct member) the eBPF verifier will block the program at
runtime.

However, some build environments don't have BTF enabled or are running
with old kernels, therefore we're vendoring `vmlinux.h` files generated
on different architectures from the iovisor/bcc project.

If `--with-pmdabpf` is to be enabled via the `./configure` script, an
architecture-specific `vmlinux.h` must be present for the platform in
the vendored libbpf-tools directory.

If missing upstream, this header will need to be regenerated and sent
to the iovisor/bcc project.  In order to do this, we run the following:

```
$ bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

This generates a complete vmlinux.h, which is 2-3MB in size.

The only use of bpftool in the PCP build is for src/pmdas/bpf/modules
skeleton code generation.
