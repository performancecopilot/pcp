# pylint doesn't recognize subprocess module of asyncio, see https://github.com/PyCQA/pylint/issues/1469
# pylint: disable=no-member
from typing import Optional, Dict
import signal
import multiprocessing
import asyncio
import traceback
import time
from datetime import datetime, timedelta
from .models import PMDAConfig, RuntimeInfo, Script, Status, Logger, MetricType, BPFtraceError
from .parser import parse_code, process_bpftrace_output
from .utils import asyncio_get_all_tasks


class ScriptTasks:

    def __init__(self):
        self.process: Optional[asyncio.subprocess.Process] = None
        self.run_bpftrace_task: Optional[asyncio.Task] = None
        self.lock = asyncio.Lock()


class ProcessManager():

    def __init__(self, config: PMDAConfig, logger: Logger, pipe: multiprocessing.Pipe, runtime_info: RuntimeInfo):
        self.loop = asyncio.get_event_loop()
        self.loop.set_exception_handler(self.handle_exception)
        self.config = config
        self.logger = logger
        self.pipe = pipe
        self.runtime_info = runtime_info
        self.scripts: Dict[str, Script] = {}
        self.script_tasks: Dict[str, ScriptTasks] = {}
        self.running = True

    def handle_exception(self, loop, context):
        self.logger.error(f"exception in event loop: {context}")

    async def read_bpftrace_stdout(self, script: Script, script_tasks: ScriptTasks):
        data_bytes_last_value = 0
        data_bytes_time = time.time()
        measure_throughput_every = 5  # seconds

        try:
            async for line in script_tasks.process.stdout:
                script.state.data_bytes += len(line)
                now = time.time()
                if now >= data_bytes_time + measure_throughput_every:
                    throughput = (script.state.data_bytes - data_bytes_last_value) / (now - data_bytes_time)
                    if throughput > self.config.max_throughput:
                        raise BPFtraceError(f"BPFtrace output exceeds limit of "
                                            f"{self.config.max_throughput} bytes per second")
                    data_bytes_last_value = script.state.data_bytes
                    data_bytes_time = time.time()

                line = line.decode('utf-8')
                try:
                    process_bpftrace_output(self.runtime_info, script, line)
                except Exception:  # pylint: disable=broad-except
                    self.logger.error(f"Error parsing bpftrace output, please open a bug report:\n"
                                      f"While reading:\n"
                                      f"{repr(line)}\n"
                                      f"the following error occured:\n"
                                      f"{traceback.format_exc()}")
        except ValueError:
            # thrown if the output exceeds 'limit' (argument passed to create_subprocess_exec)
            raise BPFtraceError(
                f"BPFtrace output exceeds limit of {self.config.max_throughput}"
                f" bytes per second") from None

    async def read_bpftrace_stderr(self, script: Script, script_tasks: ScriptTasks):
        async for line in script_tasks.process.stderr:
            line = line.decode('utf-8')
            script.state.error += line

    async def stop_bpftrace_process(self, script: Script, script_tasks: ScriptTasks):
        """stops a running bpftrace process. *does not wait for run_bpftrace task to finish*"""
        self.logger.info(f"script: stopping {script}...")
        process = script_tasks.process
        script.state.status = Status.Stopping
        process.send_signal(signal.SIGINT)

        # wait max. 5s for graceful termination of the bpftrace process
        try:
            # From https://stackoverflow.com/a/53247626/3702377 ...
            # The create_task top-level function was added in Python 3.7.
            # Prior to 3.7, create_task was only available as a method on
            # the event loop, so ...
            #
            loop = asyncio.get_event_loop()
            terminate = loop.create_task(process.wait())
            await asyncio.wait_for(terminate, timeout=5)
        except asyncio.TimeoutError:
            self.logger.info(f"stop: {script} is still running, sending SIGKILL...")
            process.kill()

            # wait again max. 5s until bpftrace process is terminated
            try:
                # see note above re. create_task()
                #
                loop = asyncio.get_event_loop()
                terminate = loop.create_task(process.wait())
                await asyncio.wait_for(terminate, timeout=5)
            except asyncio.TimeoutError:
                self.logger.info(f"stop: {script} is still running after sending SIGKILL...")

        # stopping state change in run_bpftrace task (script can also stop itself, without getting SIGINT)
        # do not await for run_bpftace task here, as run_bpftrace is awaiting for this task in case of an exception

    async def stop_bpftrace(self, script: Script, script_tasks: ScriptTasks):
        """stops a running bpftrace *and* waits for run_bpftrace task to finish"""
        await self.stop_bpftrace_process(script, script_tasks)
        await script_tasks.run_bpftrace_task

    async def run_bpftrace(self, script: Script, script_tasks: ScriptTasks):
        """runs a bpftrace process until it exits or encounters an error"""
        process = script_tasks.process
        try:
            await asyncio.gather(
                self.read_bpftrace_stdout(script, script_tasks),
                self.read_bpftrace_stderr(script, script_tasks)
            )
        except BPFtraceError as e:
            await self.stop_bpftrace_process(script, script_tasks)
            script.state.error = str(e)
            script.state.exit_code = process.returncode
            script.state.status = Status.Error
        else:
            script.state.exit_code = await process.wait()
            script.state.status = Status.Stopped if script.state.exit_code == 0 else Status.Error

        if script.state.status == Status.Error:
            self.logger.info(f"script: stopped {script} due to error: {script.state.error.rstrip()}")
        else:
            self.logger.info(f"script: stopped {script}")

    async def start_bpftrace(self, script: Script, script_tasks: ScriptTasks):
        """starts a bpftrace process. *does not wait until it is finished*"""
        self.logger.info(f"script: starting {script}...")
        script.state.reset()
        script.state.status = Status.Starting
        if script.metadata.custom_output_block:
            code = script.code
        else:
            print_stmts = ' '.join([f"print({var_name});"
                                    for var_name, var_def in script.variables.items()
                                    if var_def.metrictype != MetricType.Output])
            code = script.code + f"\ninterval:s:1 {{ {print_stmts} }}"

        try:
            # support for reading scripts on stdin arrived in bpftrace v0.11.0
            if self.runtime_info.bpftrace_version >= (0, 11, 0):
                # read scripts from stdin to not clobber ps(1) output
                script_tasks.process = await asyncio.subprocess.create_subprocess_exec(
                    self.config.bpftrace_path, '-f', 'json', '-',
                    limit=self.config.max_throughput, stdin=asyncio.subprocess.PIPE,
                    stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
                )
                script_tasks.process.stdin.write(code.encode('utf-8'))
                await script_tasks.process.stdin.drain()
                script_tasks.process.stdin.close()
            else:
                script_tasks.process = await asyncio.subprocess.create_subprocess_exec(
                    self.config.bpftrace_path, '-f', 'json', '-e', code,
                    limit=self.config.max_throughput, stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE
                )
        except OSError as e:
            script.state.error = str(e)
            script.state.exit_code = script_tasks.process.returncode if script_tasks.process else 1
            script.state.status = Status.Error
            self.logger.info(f"script: failed to start {script} due to error: {script.state.error.rstrip()}")
        else:
            script.state.status = Status.Started
            script.state.pid = script_tasks.process.pid
            script_tasks.run_bpftrace_task = asyncio.ensure_future(self.run_bpftrace(script, script_tasks))
            self.logger.info(f"script: started {script}")

    def register(self, script: Script):
        try:
            script = parse_code(script)
        except BPFtraceError as e:
            script.state.error = str(e)
            script.state.status = Status.Error
            self.pipe.send(script)
            return

        if script.metadata.name:
            for s in self.scripts.values():
                if s.metadata.name == script.metadata.name:
                    script.state.error = f"Script name {script.metadata.name} is already in use by another script."
                    script.state.status = Status.Error
                    self.logger.info(f"script: failed to start {script} due to error: {script.state.error}")
                    self.pipe.send(script)
                    return

        script.state.status = Status.Starting  # starting (soon), required for grafana-pcp
        script_tasks = ScriptTasks()
        self.scripts[script.script_id] = script
        self.script_tasks[script.script_id] = script_tasks
        asyncio.ensure_future(self.start_bpftrace(script, script_tasks))
        self.pipe.send(script)

    async def start(self, script_id: str):
        script_tasks = self.script_tasks.get(script_id)
        if not script_tasks:
            self.logger.error(f"start: script {script_id} not found")
            return

        async with script_tasks.lock:
            # script could have been deleted while waiting for the lock
            script = self.scripts.get(script_id)
            if not script:
                self.logger.error(f"start: script {script_id} not found")
                return

            if script.state.status == Status.Started:
                self.logger.info(f"start: {script} already started")
            elif script.state.status == Status.Stopped:
                await self.start_bpftrace(self.scripts[script_id], self.script_tasks[script_id])
            else:
                self.logger.error(f"start: invalid state {script.state.status} for {script}")

    async def deregister(self, script_id: str):
        script_tasks = self.script_tasks.get(script_id)
        if not script_tasks:
            self.logger.error(f"deregister: script {script_id} not found")
            return

        async with script_tasks.lock:
            # script could have been deleted while waiting for the lock
            script = self.scripts.get(script_id)
            if not script:
                self.logger.error(f"deregister: script {script_id} not found")
                return

            if script.state.status == Status.Started:
                await self.stop_bpftrace(script, script_tasks)

            if script.state.status in [Status.Stopped, Status.Error]:
                del self.scripts[script.script_id]
                del self.script_tasks[script.script_id]
                self.logger.info(f"script: deregistered {script}")
            else:
                self.logger.error(f"deregister: invalid state {script.state.status} for {script}")

    async def stop(self, script_id: str):
        script_tasks = self.script_tasks.get(script_id)
        if not script_tasks:
            self.logger.error(f"stop: script {script_id} not found")
            return

        async with script_tasks.lock:
            # script could have been deleted while waiting for the lock
            script = self.scripts.get(script_id)
            if not script:
                self.logger.error(f"stop: script {script_id} not found")
                return

            if script.state.status in [Status.Stopped, Status.Error]:
                self.logger.info(f"stop: already stopped {script}")
            elif script.state.status == Status.Started:
                await self.stop_bpftrace(script, script_tasks)
            else:
                self.logger.error(f"stop: invalid state {script.state.status} for {script}")

    def refresh(self, script_id: str):
        script = self.scripts.get(script_id)
        if not script:
            self.logger.error(f"refresh: script {script_id} not found")
        else:
            script.last_accessed_at = datetime.now()
        self.pipe.send(script)

    def list_scripts(self):
        self.pipe.send(list(self.scripts.keys()))

    async def main_loop(self):
        while self.running:
            cmd = await self.loop.run_in_executor(None, self.pipe.recv)
            if cmd is None:
                await self.shutdown()
                break

            if cmd[0] in ['register', 'refresh', 'list_scripts']:
                # foreground tasks (with response)
                try:
                    getattr(self, cmd[0])(*cmd[1:])
                except Exception:  # pylint: disable=broad-except
                    self.logger.error(f"exception in main loop: {traceback.format_exc()}")
            elif cmd[0] in ['deregister', 'start', 'stop']:
                # background tasks (no response)
                asyncio.ensure_future(getattr(self, cmd[0])(*cmd[1:]))

    async def expiry_timer(self):
        while self.running:
            script_expiry = datetime.now() - timedelta(seconds=self.config.script_expiry_time)
            # copy list of scripts here as we're modifying it during iteration
            for script in list(self.scripts.values()):
                if script.persistent or script.last_accessed_at >= script_expiry:
                    continue

                self.logger.info(f"script: deregistering {script} "
                                 f"(wasn't requested in the last {self.config.script_expiry_time} seconds)")
                await self.deregister(script.script_id)

            await asyncio.sleep(1)

    async def shutdown(self):
        self.logger.info("manager: shutting down pmdabpftrace process manager...")
        self.running = False

        # copy list of scripts here as we're modifying it during iteration
        for script in list(self.scripts.values()):
            await self.deregister(script.script_id)

    def run(self):
        self.logger.info("manager: started pmdabpftrace process manager")
        if self.runtime_info.bpftrace_version == (999, 999, 999):
            self.logger.info(f"manager: WARNING: unrecognized bpftrace version "
                             f"{self.runtime_info.bpftrace_version_str}, assuming latest version")
        else:
            self.logger.info(f"manager: using bpftrace {self.runtime_info.bpftrace_version_str}")

        asyncio.ensure_future(self.expiry_timer())
        self.loop.run_until_complete(self.main_loop())

        # stop pending tasks
        pending = asyncio_get_all_tasks(self.loop)
        if pending:
            self.logger.info("manager: waiting 10 secs for running tasks to stop...")
            self.loop.run_until_complete(asyncio.wait(pending, timeout=10))

            pending = asyncio_get_all_tasks(self.loop)
            for task in pending:
                task.cancel()
                try:
                    self.loop.run_until_complete(task)
                except asyncio.CancelledError:
                    pass

        self.loop.close()
        self.logger.info("manager: shutdown pmdabpftrace process manager")
        self.pipe.send(None)
