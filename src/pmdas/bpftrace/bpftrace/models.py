from typing import Optional, List, Dict
import uuid
import json
from datetime import datetime


class BPFtraceError(Exception):
    """BPFtrace general error"""


class MetricType:
    """BPFtrace variable types"""
    Control = 'control'
    Histogram = 'histogram'
    Stacks = 'stacks'
    Output = 'output'


class VariableDefinition:
    def __init__(self, single: bool, semantics: int, datatype: int, metrictype: MetricType):
        self.single = single
        self.semantics = semantics
        self.datatype = datatype
        self.metrictype = metrictype


class ScriptMetadata:
    def __init__(self):
        self.name: Optional[str] = None
        self.include: Optional[List[str]] = None
        self.table_retain_lines: Optional[int] = None


class Status:
    Stopped = 'stopped'
    Starting = 'starting'  # starting can take a while
    Started = 'started'
    Stopping = 'stopping'  # if the process doesn't respond to SIGINT, wait 5s for SIGKILL
    Error = 'error'  # stopped and error occured (bpftrace error or process manager error)


class State:
    def __init__(self):
        self.status = Status.Stopped
        self.reset()

    def reset(self):
        self.pid = -1
        self.exit_code = 0
        self.error = ''
        self.probes = 0
        self.data = {}


class Script:
    def __init__(self, code: str):
        # PMNS metric names must start with an alphabetic character
        self.script_id = 's' + str(uuid.uuid4()).replace('-', '')
        self.username: Optional[str] = None
        self.persistent = False
        self.created_at = datetime.now()
        self.last_accessed_at = datetime.now()
        self.code = code
        self.metadata = ScriptMetadata()
        self.variables: Dict[str, VariableDefinition] = {}
        self.state = State()

    def __str__(self) -> str:
        pid_str = f" (PID={self.state.pid})" if self.state.pid != -1 else ""
        return f"script {self.script_id}{pid_str}"


class ScriptEncoder(json.JSONEncoder):
    def __init__(self, *args, dump_state_data=True, **kwargs):
        super().__init__(*args, **kwargs)
        self.dump_state_data = dump_state_data

    # pylint: disable=arguments-differ,method-hidden
    def default(self, obj):
        if isinstance(obj, State) and not self.dump_state_data:
            state = obj.__dict__.copy()
            del state["data"]
            return state
        elif isinstance(obj, (Script, ScriptMetadata, VariableDefinition, State)):
            return obj.__dict__
        elif isinstance(obj, datetime):
            return obj.isoformat()
        else:
            return json.JSONEncoder.default(self, obj)


class PMDAConfig:
    # see bpftrace.conf for configuration descriptions and units
    class Authentication:
        def __init__(self):
            self.enabled = True
            self.allowed_users = []

    def __init__(self):
        self.authentication = PMDAConfig.Authentication()

        self.bpftrace_path = 'bpftrace'
        self.script_expiry_time = 60  # 1 min
        self.max_throughput = 2 * 1024 * 1024  # 2 MB/s


class RuntimeInfo:
    def __init__(self):
        # assuming latest version per default (simplifies version checks)
        self.bpftrace_version = (999, 999, 999)
        self.bpftrace_version_str = ""


class Logger:
    def __init__(self, info, error):
        self.info = info
        self.error = error
