from typing import Optional, List
import multiprocessing
from .models import Script, PMDAConfig, RuntimeInfo, Logger, BPFtraceError
from .process_manager import ProcessManager
from .utils import get_bpftrace_version


def process_manager_main(config: PMDAConfig, logger: Logger, pipe: multiprocessing.Pipe, runtime_info: RuntimeInfo):
    ProcessManager(config, logger, pipe, runtime_info).run()


class BPFtraceService():

    def __init__(self, config: PMDAConfig, logger: Logger):
        self.config = config
        self.logger = logger
        self.pipe, self.child_pipe = multiprocessing.Pipe()
        self.process = None

    def wait_for_response(self, timeout: int, request='?'):
        if self.pipe.poll(timeout):
            return self.pipe.recv()
        else:
            self.logger.error(f"process manager did not reply in {timeout} seconds for {request}")
            return None

    def gather_runtime_info(self) -> RuntimeInfo:
        runtime_info = RuntimeInfo()

        try:
            runtime_info.bpftrace_version_str, runtime_info.bpftrace_version = \
                get_bpftrace_version(self.config.bpftrace_path)
        except OSError as e:
            # file not found, insufficient permissions, ...
            raise BPFtraceError(f"Error starting bpftrace: {e}")

        if runtime_info.bpftrace_version < (0, 9, 2):
            raise BPFtraceError("bpftrace version 0.9.2 or higher is required "
                                "for this PMDA (current version: {runtime_info.bpftrace_version_str})")
        return runtime_info

    def start_daemon(self):
        if self.process:
            self.logger.error("pmdabpftrace process manager is already started")
            return

        runtime_info = self.gather_runtime_info()
        self.process = multiprocessing.Process(name="pmdabpftrace process manager",
                                               target=process_manager_main,
                                               args=(self.config,
                                                     self.logger, self.child_pipe,
                                                     runtime_info),
                                               daemon=True)
        self.process.start()

    def stop_daemon(self):
        if not self.process:
            self.logger.error("pmdabpftrace process manager is already stopped")
            return

        # stops the main loop
        self.pipe.send(None)

        # after the main loop is stopped and all pending tasks are completed, ProcessManager sends None over the pipe
        self.pipe.recv()
        self.process = None

    def send_request(self, request: tuple, wait=None):
        self.pipe.send(request)
        if not wait:
            return None

        if self.pipe.poll(wait):
            return self.pipe.recv()
        else:
            self.logger.error(f"process manager did not reply in {wait} seconds for {request}")
            return None

    def register_script(self, script: Script) -> Script:
        return self.send_request(('register', script), wait=2)

    def deregister_script(self, script_id: str):
        return self.send_request(('deregister', script_id))

    def start_script(self, script_id: str):
        return self.send_request(('start', script_id))

    def stop_script(self, script_id: str):
        return self.send_request(('stop', script_id))

    def refresh_script(self, script_id: str) -> Optional[Script]:
        return self.send_request(('refresh', script_id), wait=2)

    def list_scripts(self) -> Optional[List[str]]:
        return self.send_request(('list_scripts',), wait=2)
