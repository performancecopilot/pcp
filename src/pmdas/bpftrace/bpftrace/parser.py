from typing import Dict
import re
import json
from cpmapi import PM_SEM_INSTANT, PM_SEM_COUNTER, PM_TYPE_U64, PM_TYPE_STRING
from .models import RuntimeInfo, Script, BPFtraceError, VariableDefinition, MetricType


class BPFtraceMessageType:
    """BPFtrace JSON output types"""
    AttachedProbes = 'attached_probes'
    Map = 'map'
    Hist = 'hist'
    Printf = 'printf'
    Time = 'time'


def parse_code(script: Script):
    """parse bpftrace script (read variable semantics, add continuous output)"""
    found_metadata = re.findall(r'^//\s*([\w\-]+)' +  # key
                                r'(?:\s*:\s*(.+?)\s*)?$',  # optional value
                                script.code, re.MULTILINE)
    for key, val in found_metadata:
        if key == 'name':
            if re.match(r'^[a-zA-Z_]\w+$', val):
                script.metadata.name = val
            else:
                raise BPFtraceError(f"invalid value '{val}' for {key}: "
                                    f"must contain only alphanumeric characters and "
                                    f"start with a letter")
        elif key == 'include':
            script.metadata.include = val.split(',')
        elif key == 'table-retain-lines':
            if val.isdigit():
                script.metadata.table_retain_lines = int(val)
            else:
                raise BPFtraceError(f"invalid value '{val}' for {key}, "
                                    f"must be numeric")

    all_variables = re.findall(r'(@.*?)' +  # variable
                               r'(?:\[(.+?)\])?' +  # optional map key
                               r'\s*=\s*' +  # assignment
                               r'(?:([a-zA-Z]\w*)\s*\()?',  # optional function
                               script.code)
    for var, key, func in all_variables:
        if script.metadata.include is not None and var not in script.metadata.include:
            continue

        vardef = VariableDefinition(single=True, semantics=PM_SEM_INSTANT, datatype=PM_TYPE_U64,
                                    metrictype=None)
        if func in ['hist', 'lhist']:
            if key:
                raise BPFtraceError("every histogram needs to be in a separate variable")
            vardef.single = False
            vardef.semantics = PM_SEM_COUNTER
            vardef.metrictype = MetricType.Histogram
        elif func == 'count':
            vardef.single = not bool(key)
            vardef.semantics = PM_SEM_COUNTER
            if key in ['ustack', 'kstack']:
                vardef.metrictype = MetricType.Stacks
        else:
            vardef.single = not bool(key)
            vardef.semantics = PM_SEM_INSTANT
        script.variables[var] = vardef

    output_fns = re.search(r'(printf|time)\s*\(', script.code)
    if output_fns and (script.metadata.include is None or '@output' in script.metadata.include):
        if '@output' in script.variables:
            raise BPFtraceError("output from printf(), time() etc. will be stored in @output. "
                                "please rename the existing @output variable or remove any "
                                "calls to any function which produces output")
        script.variables['@output'] = VariableDefinition(single=True, semantics=PM_SEM_INSTANT,
                                                         datatype=PM_TYPE_STRING,
                                                         metrictype=MetricType.Output)

    if not script.variables:
        raise BPFtraceError("no bpftrace variables or printf statements found, please include "
                            "at least one variable or print statement in your script")
    if script.metadata.name:
        script.persistent = True
    return script


def table_retain_lines(script: Script):
    """cut table lines and keep first line (header)"""
    output = script.state.data['@output']
    newlines = []
    for i, c in enumerate(output):
        if c == '\n':
            newlines.append(i + 1)

    # e.g. if we found one NL, we have 2 lines
    if len(newlines) + 1 > 1 + script.metadata.table_retain_lines:
        # special handling if last character is a NL
        ignore_last_nl = 1 if output[-1] == '\n' else 0
        # pylint: disable=invalid-unary-operand-type
        start_content_at = newlines[-script.metadata.table_retain_lines - ignore_last_nl]
        script.state.data['@output'] = output[:newlines[0]] + output[start_content_at:]


def process_bpftrace_output_obj(runtime_info: RuntimeInfo, script: Script, obj: Dict):
    """process a single JSON object from bpftrace output"""
    if obj['type'] == BPFtraceMessageType.AttachedProbes:
        if runtime_info.bpftrace_version <= (0, 9, 2):
            script.state.probes = obj['probes']
        else:
            # https://github.com/iovisor/bpftrace/commit/9d1269b
            script.state.probes = obj['data']['probes']
    elif obj['type'] == BPFtraceMessageType.Map:
        script.state.data.update(obj['data'])
    elif obj['type'] == BPFtraceMessageType.Hist:
        for k, v in obj['data'].items():
            script.state.data[k] = {
                '{}-{}'.format(bucket.get('min', '-inf'), bucket.get('max', 'inf')): bucket['count']
                for bucket in v
            }
    elif obj['type'] in [BPFtraceMessageType.Printf, BPFtraceMessageType.Time]:
        if runtime_info.bpftrace_version <= (0, 9, 2):
            script.state.data['@output'] = script.state.data.get('@output', '') + obj['msg']
        else:
            # https://github.com/iovisor/bpftrace/commit/9d1269b
            script.state.data['@output'] = script.state.data.get('@output', '') + obj['data']
        if script.metadata.table_retain_lines is not None:
            table_retain_lines(script)


def process_bpftrace_output(runtime_info: RuntimeInfo, script: Script, line: str):
    if runtime_info.bpftrace_version <= (0, 9, 2) and '": }' in line:
        # invalid JSON, fixed in https://github.com/iovisor/bpftrace/commit/348975b
        return

    if not line or line.isspace():
        return

    obj = json.loads(line)
    process_bpftrace_output_obj(runtime_info, script, obj)
