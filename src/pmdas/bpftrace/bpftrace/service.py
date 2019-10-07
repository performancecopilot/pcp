from typing import Optional, List
import multiprocessing
from .models import Script, PMDAConfig, Logger, Status, BPFtraceError
from .process_manager import ProcessManagerDaemon
from .parser import parse_code


def process_manager_main(config: PMDAConfig, logger: Logger, pipe: multiprocessing.Pipe):
    ProcessManagerDaemon(config, logger, pipe).run()


def recv_timeout(pipe, timeout):
    if pipe.poll(timeout):
        return pipe.recv()
    else:
        return None


class BPFtraceService():

    def __init__(self, config: PMDAConfig, logger: Logger):
        self.config = config
        self.logger = logger
        self.pipe, child_pipe = multiprocessing.Pipe()
        self.process = multiprocessing.Process(name="pmdabpftrace process manager",
                                               target=process_manager_main,
                                               args=(self.config, self.logger, child_pipe),
                                               daemon=True)

    def wait_for_response(self, timeout: int, request='?'):
        if self.pipe.poll(timeout):
            return self.pipe.recv()
        else:
            self.logger.error(f"process manager did not reply in {timeout} seconds for {request}")
            return None

    def start_daemon(self):
        self.process.start()

    def stop_daemon(self):
        # stops the main loop
        self.pipe.send(None)

        # after the main loop is stopped and all pending tasks are completed, ProcessManager sends None over the pipe
        self.pipe.recv()

    def register_script(self, script: Script) -> Script:
        try:
            script = parse_code(script)
        except BPFtraceError as e:
            script.state.error = str(e)
            script.state.status = Status.Error
            return script

        script.state.status = Status.Starting
        self.pipe.send(('register', script))
        return script

    def deregister_script(self, script_id: str):
        self.pipe.send(('deregister', script_id))

    def start_script(self, script_id: str):
        self.pipe.send(('start', script_id))

    def stop_script(self, script_id: str):
        self.pipe.send(('stop', script_id))

    def refresh_script(self, script_id: str, timeout=2) -> Optional[Script]:
        self.pipe.send(('refresh', script_id))
        return self.wait_for_response(timeout, ('refresh', script_id))

    def list_scripts(self, timeout=2) -> Optional[List[str]]:
        self.pipe.send(('list_scripts',))
        return self.wait_for_response(timeout, 'list_scripts')
