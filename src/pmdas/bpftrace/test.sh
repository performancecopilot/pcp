pmstore -F bpftrace.control.register "kretprobe:vfs_read { @bytes = hist(retval); @scalar = 5; }"
