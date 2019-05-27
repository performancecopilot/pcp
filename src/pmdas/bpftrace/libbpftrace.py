from ctypes.util import find_library
from ctypes import CDLL, c_int, c_long, c_char_p, c_void_p, cast, byref
from ctypes import addressof, sizeof, POINTER, Structure, create_string_buffer


#libbpftrace = CDLL(find_library("bpftracelib"))
libbpftrace = CDLL("/home/agerstmayr/redhat/dev/pcp/src/pmdas/bpftrace/libbpftracelib.so")

libbpftrace.bpftrace_init.argtypes = []
libbpftrace.bpftrace_init.restype = c_void_p

libbpftrace.bpftrace_run.argtypes = [c_void_p, c_char_p]
libbpftrace.bpftrace_run.restype = c_int

libbpftrace.bpftrace_data.argtypes = [c_void_p, c_char_p]
libbpftrace.bpftrace_data.restype = c_int
