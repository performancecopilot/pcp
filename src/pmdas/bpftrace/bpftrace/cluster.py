from typing import Dict
import re

from pcp.pmda import PMDA, pmdaMetric
from pcp.pmapi import pmUnits
import cpmapi as c_api

from .models import Script, MetricType, VariableDefinition, Logger
from .service import BPFtraceService
from .uncached_indom import UncachedIndom


class Consts:
    class Script:
        """items of script clusters"""
        Cluster = 100  # cluster number of first script
        Status = 0
        Pid = 1
        ExitCode = 2
        Error = 3
        Probes = 4
        Code = 5
        First_BPFtrace_Variable = 10


class BPFtraceCluster:
    """manage all metrics of a single bpftrace script"""

    next_cluster_id = Consts.Script.Cluster

    def __init__(self, pmda: PMDA, logger: Logger, bpftrace_service: BPFtraceService, script: Script):
        self.pmda = pmda
        self.logger = logger
        self.bpftrace_service = bpftrace_service
        self.script = script
        self.name = script.metadata.name if script.metadata.name else script.script_id
        self.cluster_id = BPFtraceCluster.next_cluster_id
        BPFtraceCluster.next_cluster_id += 1

        self.pmid_to_var: Dict[int, str] = {}
        self.item_to_var: Dict[int, str] = {}
        self.indoms: Dict[str, UncachedIndom] = {}
        self.metrics = {}

    @classmethod
    def normalize_variable_name(cls, var_name: str) -> str:
        """normalize bpftrace variable name to conform PCP metric names"""
        metric_name = var_name[1:]
        return metric_name if metric_name else 'root'

    def register_metrics(self):
        """register metrics for this bpftrace instance"""
        self.status_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.Status), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.status', self.status_metric,
                             "status of bpftrace script")

        self.pid_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.Pid), c_api.PM_TYPE_U32,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.pid', self.pid_metric,
                             "pid of bpftrace process")

        self.exit_code_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.ExitCode), c_api.PM_TYPE_32,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.exit_code',
                             self.exit_code_metric, "exit code of bpftrace process")

        self.error_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.Error), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.error', self.error_metric,
                             "errors of the bpftrace script")

        self.probes_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.Probes), c_api.PM_TYPE_U32,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.probes', self.probes_metric,
                             "number of attached probes")

        self.code_metric = pmdaMetric(
            self.pmda.pmid(self.cluster_id, Consts.Script.Code), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        )
        self.pmda.add_metric(f'bpftrace.scripts.{self.name}.code', self.code_metric,
                             "bpftrace script")

        item_no = Consts.Script.First_BPFtrace_Variable
        for var_name, var_def in self.script.variables.items():
            if var_def.single:
                indom_id = c_api.PM_INDOM_NULL
            else:
                # serial needs to be unique across PMDA
                indom = UncachedIndom(self.pmda, self.cluster_id * 1000 + item_no)
                indom_id = indom.indom_id
                self.indoms[var_name] = indom

            pmid = self.pmda.pmid(self.cluster_id, item_no)
            metric = pmdaMetric(pmid, var_def.datatype, indom_id, var_def.semantics, pmUnits())
            normalized_var_name = self.normalize_variable_name(var_name)
            metric_name = f'bpftrace.scripts.{self.name}.data.{normalized_var_name}'
            self.pmda.add_metric(metric_name, metric, f"{var_name} variable of bpftrace script")
            self.metrics[metric_name] = metric
            self.pmid_to_var[pmid] = var_name
            self.item_to_var[item_no] = var_name
            item_no += 1

    def deregister_metrics(self):
        """remove bpftrace metrics for this bpftrace instance"""
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.status', self.status_metric)
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.pid', self.pid_metric)
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.exit_code', self.exit_code_metric)
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.error', self.error_metric)
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.probes', self.probes_metric)
        self.pmda.remove_metric(f'bpftrace.scripts.{self.name}.code', self.code_metric)
        for metric_name, metric in self.metrics.items():
            self.pmda.remove_metric(metric_name, metric)

    def label(self, ident: int, type_):
        """PMDA label"""
        if type_ == c_api.PM_LABEL_ITEM:
            if ident in [Consts.Script.Status, Consts.Script.Pid, Consts.Script.ExitCode,
                         Consts.Script.Error, Consts.Script.Probes, Consts.Script.Code]:
                return f'{{"metrictype":"{MetricType.Control}"}}'
            elif ident in self.pmid_to_var:
                var_name = self.pmid_to_var[ident]
                metrictype = self.script.variables[var_name].metrictype
                if metrictype:
                    return f'{{"metrictype":"{metrictype}"}}'
        return '{}'

    @classmethod
    def instance_name_sorting_key(cls, var_def: VariableDefinition):
        if var_def.metrictype == MetricType.Histogram:
            regex = re.compile(r'^(.+?)\-(.+?)$')

            def sort_key(val):
                m = regex.match(val)
                # float('-inf') returns negative infinity, int('-inf') doesn't
                return float(m.group(1)) if m else val
            return sort_key
        else:
            return None

    def refresh_callback(self):
        """PMDA refresh callback for this bpftrace instance"""
        script = self.bpftrace_service.refresh_script(self.script.script_id)
        if not script:
            return

        self.script = script
        # refresh instance domains
        for var_name, var_def in self.script.variables.items():
            # parser found variable definition in script,
            # but maybe variable doesn't exist in bpftrace output due to an error
            if not var_def.single and var_name in self.script.state.data:
                self.indoms[var_name].update(self.script.state.data[var_name].keys(),
                                             key_fn=self.instance_name_sorting_key(var_def))

    def fetch_callback(self, item: int, inst: int):
        """
        PMDA fetch callback for this bpftrace instance

        if a client queries a bpftrace map value, but it doesn't exist yet,
        we return PM_ERR_VALUE, which shows up in the logs as:
        Error: pmdaFetch: Fetch callback error from metric PMID <...>: Missing metric value(s)
        """
        if item == Consts.Script.Status:
            return [self.script.state.status, 1]
        elif item == Consts.Script.Pid:
            return [self.script.state.pid, 1]
        elif item == Consts.Script.ExitCode:
            return [self.script.state.exit_code, 1]
        elif item == Consts.Script.Error:
            return [self.script.state.error.strip(), 1]
        elif item == Consts.Script.Probes:
            return [self.script.state.probes, 1]
        elif item == Consts.Script.Code:
            return [self.script.code, 1]
        elif item in self.item_to_var:
            var_name = self.item_to_var[item]
            if var_name not in self.script.state.data:
                return [c_api.PM_ERR_VALUE, 0]

            if self.script.variables[var_name].single:
                return [self.script.state.data[var_name], 1]
            else:
                key = self.indoms[var_name].inst_name_lookup(inst)
                if key is not None and key in self.script.state.data[var_name]:
                    return [self.script.state.data[var_name][key], 1]
                return [c_api.PM_ERR_INST, 0]
        return [c_api.PM_ERR_PMID, 0]
