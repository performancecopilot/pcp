# pylint doesn't recognize subprocess module of asyncio, see https://github.com/PyCQA/pylint/issues/1469
# pylint: disable=no-member
from typing import Optional, Dict
import signal
import multiprocessing
import asyncio
import traceback
import time
from datetime import datetime, timedelta
from .models import PMDAConfig, Script, Status, Logger, MetricType, BPFtraceError
from .parser import process_bpftrace_output


class ScriptTasks:

    def __init__(self):
        self.process: Optional[asyncio.subprocess.Process] = None
        self.run_bpftrace_task: Optional[asyncio.Task] = None


class ProcessManagerDaemon():

    def __init__(self, config: PMDAConfig, logger: Logger, pipe: multiprocessing.Pipe):
        self.loop = asyncio.get_event_loop()
        self.loop.set_exception_handler(self.handle_exception)
        self.config = config
        self.logger = logger
        self.pipe = pipe
        self.scripts: Dict[str, Script] = {}
        self.script_tasks: Dict[str, ScriptTasks] = {}

    def handle_exception(self, loop, context):
        self.logger.error(f"exception in event loop: {context}")

    async def read_bpftrace_stdout(self, process: asyncio.subprocess.Process, script: Script):
        read_bytes = 0
        read_bytes_start = time.time()

        try:
            async for line in process.stdout:
                read_bytes += len(line)
                now = time.time()
                if now >= read_bytes_start + 3:
                    throughput = read_bytes / (now - read_bytes_start)
                    if throughput > self.config.max_throughput:
                        raise BPFtraceError(f"BPFtrace output exceeds limit of "
                                            f"{self.config.max_throughput} bytes per second")
                    read_bytes = 0
                    read_bytes_start = time.time()

                line = line.decode('utf-8')
                process_bpftrace_output(self.config, script, line)
        except ValueError:
            raise BPFtraceError(
                f"BPFtrace output exceeds limit of {self.config.max_throughput}"
                f" bytes per second") from None

    async def read_bpftrace_stderr(self, process: asyncio.subprocess.Process, script: Script):
        async for line in process.stderr:
            line = line.decode('utf-8')
            script.state.error += line

    async def stop_bpftrace(self, process: asyncio.subprocess.Process, script: Script):
        self.logger.info(f"stopping {script.ident()}...")
        script.state.status = Status.Stopping
        process.send_signal(signal.SIGINT)

        # wait max. 5s for graceful termination of the bpftrace process
        _done, pending = await asyncio.wait({process.wait()}, timeout=5)
        if pending:
            self.logger.info(f"stop: process {script.ident()} is still running, sending SIGKILL...")
            process.kill()

            # wait again max. 5s until bpftrace process is terminated
            _done, pending = await asyncio.wait({process.wait()}, timeout=5)
            if pending:
                self.logger.info(f"stop: process {script.ident()} is still running after sending SIGKILL...")

    async def run_bpftrace(self, script: Script, script_tasks: ScriptTasks):
        print_stmts = ' '.join([f"print({var_name});"
                                for var_name, var_def in script.variables.items()
                                if var_def.metrictype != MetricType.Output])
        code = script.code + f"\ninterval:s:1 {{ {print_stmts} }}"

        process = await asyncio.subprocess.create_subprocess_exec(
            self.config.bpftrace_path, '-f', 'json', '-e', code,
            limit=self.config.max_throughput, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
        )
        script_tasks.process = process
        script.state.status = Status.Started
        script.state.pid = process.pid
        self.logger.info(f"started {script.ident()}.")

        try:
            await asyncio.gather(self.read_bpftrace_stdout(process, script), self.read_bpftrace_stderr(process, script))
        except BPFtraceError as e:
            await self.stop_bpftrace(process, script)
            script.state.error = str(e)
            script.state.exit_code = process.returncode
            script.state.status = Status.Error
        else:
            script.state.exit_code = await process.wait()
            script.state.status = Status.Stopped if script.state.exit_code == 0 else Status.Error

        self.logger.info(f"stopped {script.ident()}")

    async def register(self, script: Script):
        self.scripts[script.script_id] = script
        self.script_tasks[script.script_id] = ScriptTasks()
        await self.start(script.script_id)

    async def start(self, script_id: str):
        script = self.scripts.get(script_id)
        if not script:
            self.logger.error(f"start: script {script_id} not found")
            return

        if script.state == Status.Started:
            self.logger.info(f"start: script {script_id} already started")
        elif script.state in [Status.Starting, Status.Stopping]:
            self.logger.info(f"start: script {script_id} is {script.state}")
        else:  # stopped, error
            script.state.reset()
            script.state.status = Status.Starting
            script_tasks = self.script_tasks[script.script_id]
            script_tasks.run_bpftrace_task = asyncio.ensure_future(self.run_bpftrace(script, script_tasks))
            await script_tasks.run_bpftrace_task

    async def deregister(self, script_id: str):
        script = self.scripts.get(script_id)
        if not script:
            self.logger.error(f"deregister: script {script_id} not found")
            return

        if script.state in [Status.Starting, Status.Stopping]:
            self.logger.info(f"deregister: script {script_id} is {script.state}")
            return
        elif script.state.status == Status.Started:
            await self.stop(script_id)

        # status = stopped, error
        del self.scripts[script.script_id]
        del self.script_tasks[script.script_id]
        self.logger.info(f"removed script {script.ident()}")

    async def stop(self, script_id: str):
        script = self.scripts.get(script_id)
        if not script:
            self.logger.error(f"stop: script {script_id} not found")
            return

        if script.state.status in [Status.Stopping, Status.Stopped, Status.Error]:
            self.logger.info(f"stop: already stopped {script_id}")
            return

        # status = starting, started
        script_tasks = self.script_tasks[script_id]
        await self.stop_bpftrace(script_tasks.process, script)
        await script_tasks.run_bpftrace_task

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
        while True:
            cmd = await self.loop.run_in_executor(None, self.pipe.recv)
            if cmd is None:
                await self.shutdown()
                break

            if cmd[0] in ['register', 'deregister', 'start', 'stop']:
                asyncio.ensure_future(getattr(self, cmd[0])(*cmd[1:]))
            elif cmd[0] in ['refresh', 'list_scripts']:
                try:
                    getattr(self, cmd[0])(*cmd[1:])
                except:  # pylint: disable=bare-except
                    self.logger.error(f"exception in main loop: {traceback.format_exc()}")

    async def expiry_timer(self):
        while True:
            script_expiry = datetime.now() - timedelta(seconds=self.config.script_expiry_time)
            # copy list of scripts here as we're modifying it during iteration
            for script in list(self.scripts.values()):
                if script.persistent or script.last_accessed_at >= script_expiry:
                    continue

                self.logger.info(f"deregistering script {script.ident()} "
                                 f"(wasn't requested in the last {self.config.script_expiry_time} seconds)")
                await self.deregister(script.script_id)

            await asyncio.sleep(1)

    async def shutdown(self):
        self.logger.info("shutting down bpftrace process manager daemon...")
        # copy list of scripts here as we're modifying it during iteration
        for script in list(self.scripts.values()):
            await self.deregister(script.script_id)
        self.logger.info("shutdown bpftrace process manager daemon.")

    def run(self):
        self.logger.info("started bpftrace process manager daemon.")
        asyncio.ensure_future(self.expiry_timer())
        self.loop.run_until_complete(self.main_loop())

        # stop pending tasks
        pending = asyncio.Task.all_tasks()
        for task in pending:
            task.cancel()
            try:
                self.loop.run_until_complete(task)
            except asyncio.CancelledError:
                pass

        self.loop.close()
        self.pipe.send(None)
