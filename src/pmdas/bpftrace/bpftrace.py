import os
import signal
from multiprocessing import Process, Queue
from threading import Thread
import ctypes
from libbpftrace import libbpftrace


class BPFTrace(Process):
    def __init__(self, log, script):
        super().__init__()
        self.log = log
        self.script = script
        self.q = Queue()

    def comm_thread(self, bpftrace_ptr):
        """thread in newly created process"""
        while True:
            cmd = self.q.get()
            if cmd == 'data':
                strbuf = ctypes.create_string_buffer(1024*10)
                libbpftrace.bpftrace_data(bpftrace_ptr, strbuf)
                self.q.put(strbuf.value.decode('utf-8'))
            elif cmd == 'quit':
                break

    def run(self):
        """new process"""
        bpftrace_ptr = libbpftrace.bpftrace_init()

        t = Thread(target=self.comm_thread, args=(bpftrace_ptr,))
        t.start()

        self.log("starting BPFtrace script {}".format(self.script))
        err = libbpftrace.bpftrace_run(bpftrace_ptr, self.script.encode('utf-8'))
        if err:
            self.log("error running bpftrace {}".format(err))
        else:
            self.log("cleanup successful")

    def data(self):
        self.q.put('data')
        return self.q.get()

    def stop(self, wait=False):
        self.q.put('quit')
        os.kill(self.pid, signal.SIGINT)
        if wait:
            self.join()
