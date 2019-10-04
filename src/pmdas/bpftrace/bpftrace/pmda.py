from typing import Optional, Any, Dict
import os
import sys
import json
import configparser
import atexit

import cpmda
from pcp.pmda import PMDA, pmdaMetric, pmdaGetContext
from pcp.pmapi import pmUnits, pmContext as PCP
import cpmapi as c_api

from .models import PMDAConfig, Logger, Status, Script, ScriptEncoder, MetricType
from .utils import get_bpftrace_version, get_tracepoints_csv
from .service import BPFtraceService
from .cluster import BPFtraceCluster
from .uncached_indom import UncachedIndom


class Consts:  # pylint: disable=too-few-public-methods
    """PMDA constants"""
    class Control:
        """cluster number and items of control cluster"""
        Cluster = 0  # cluster number
        Register = 0  # items
        Deregister = 1
        Start = 2
        Stop = 3

    class Info:
        """cluster number and items of info cluster"""
        Cluster = 1  # cluster number
        Scripts = 0  # items
        ScriptsJson = 1
        Tracepoints = 2


class BPFtracePMDA(PMDA):

    def __init__(self, name: str, domain: int):
        super(BPFtracePMDA, self).__init__(name, domain)

        self.config = self.parse_config()
        self.logger = Logger(self.log, self.err)
        self.bpftrace_service = BPFtraceService(self.config, self.logger)

        if not self.is_pmda_setup():
            version_str = ''
            version = None
            try:
                version_str, version = get_bpftrace_version(self.config.bpftrace_path)
            except OSError as e:
                # file not found, insufficient permissions, ...
                self.logger.error(f"Error starting bpftrace: {e}")
                sys.exit(1)

            if not version:
                self.logger.info(f"WARNING: unrecognized bpftrace version {version_str}, "
                                 "assuming latest version")
            elif version < (0, 9, 2):
                self.logger.error("bpftrace version 0.9.2 or higher is required "
                                  "for this PMDA (current version: {version_str})")
                sys.exit(1)
            else:
                self.logger.info(f"using {version_str}")
                self.config.bpftrace_version = version

            self.tracepoints_csv = get_tracepoints_csv(self.config.bpftrace_path)
            self.bpftrace_service.start_daemon()

        self.set_comm_flags(cpmda.PMDA_FLAG_AUTHORIZE)
        self.connect_pmcd()

        self.ctxtab: Dict[int, Dict[str, Any]] = {}
        self.clusters: Dict[int, BPFtraceCluster] = {}
        self.script_indom = UncachedIndom(self, 0)

        self.register_metrics()
        self.set_attribute_callback(self.attribute_callback)
        self.set_label(self.label)
        self.set_store_callback(self.store_callback)
        self.set_refresh(self.refresh_callback)
        self.set_fetch_callback(self.fetch_callback)

        @atexit.register
        def cleanup():  # pylint: disable=unused-variable
            if not self.is_pmda_setup:
                self.logger.info("Cleanup...")
                self.bpftrace_service.stop_daemon()

    def is_pmda_setup(self):
        """checks if PMDA is in setup state"""
        return os.environ.get('PCP_PYTHON_DOMAIN') or os.environ.get('PCP_PYTHON_PMNS')

    def parse_config(self) -> PMDAConfig:
        pmdas_dir = PCP.pmGetConfig('PCP_PMDAS_DIR')
        configfile = f"{pmdas_dir}/{self.name}/{self.name}.conf"

        # Python < 3.2 compat #
        if sys.version_info >= (3, 2):
            configreader = configparser.ConfigParser()
        else:
            configreader = configparser.SafeConfigParser()

        # read() will skip missing files
        configreader.read(configfile)

        config = PMDAConfig()
        if configreader.has_section('pmda'):
            for opt in configreader.options('pmda'):
                if opt == 'bpftrace_path':
                    config.bpftrace_path = configreader.get('pmda', opt)
                elif opt == 'script_expiry_time':
                    config.script_expiry_time = configreader.getint('pmda', opt)
                elif opt == 'max_throughput':
                    config.max_throughput = configreader.getint('pmda', opt)
                elif opt == 'allowed_users':
                    config.allowed_users = configreader.get('pmda', opt).split(',')
                else:
                    self.logger.error(f"Invalid directive '{opt}' in {configfile}, aborting.")
                    sys.exit(1)
        return config

    def register_metrics(self):
        """register control metrics"""
        self.add_metric('bpftrace.control.register', pmdaMetric(
            self.pmid(Consts.Control.Cluster, Consts.Control.Register), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "register a new bpftrace script")

        self.add_metric('bpftrace.control.deregister', pmdaMetric(
            self.pmid(Consts.Control.Cluster, Consts.Control.Deregister), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "deregister a bpftrace script")

        self.add_metric('bpftrace.control.start', pmdaMetric(
            self.pmid(Consts.Control.Cluster, Consts.Control.Start), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "start a stopped bpftrace script")

        self.add_metric('bpftrace.control.stop', pmdaMetric(
            self.pmid(Consts.Control.Cluster, Consts.Control.Stop), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "stop a running bpftrace script")

        self.add_metric('bpftrace.info.scripts', pmdaMetric(
            self.pmid(Consts.Info.Cluster, Consts.Info.Scripts), c_api.PM_TYPE_STRING,
            self.script_indom.indom_id, c_api.PM_SEM_INSTANT, pmUnits()
        ), "list all registered bpftrace scripts")

        self.add_metric('bpftrace.info.scripts_json', pmdaMetric(
            self.pmid(Consts.Info.Cluster, Consts.Info.ScriptsJson), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "expose all scripts in JSON format")

        self.add_metric('bpftrace.info.tracepoints', pmdaMetric(
            self.pmid(Consts.Info.Cluster, Consts.Info.Tracepoints), c_api.PM_TYPE_STRING,
            c_api.PM_INDOM_NULL, c_api.PM_SEM_INSTANT, pmUnits()
        ), "list all available tracepoints")

    def set_ctx_state(self, key: str, value: Any):
        """set per-context state"""
        ctx = pmdaGetContext()
        if ctx not in self.ctxtab:
            self.ctxtab[ctx] = {}
        self.ctxtab[ctx][key] = value

    def get_ctx_state(self, key: str, default=None, delete=False) -> Any:
        """get (and delete) per-context state"""
        ctx = pmdaGetContext()
        if ctx not in self.ctxtab or key not in self.ctxtab[ctx]:
            return default

        value = self.ctxtab[ctx].get(key, default)
        if delete:
            del self.ctxtab[ctx][key]
        return value

    def attribute_callback(self, ctx, attr, value):
        if attr == cpmda.PMDA_ATTR_USERNAME:
            self.log("username"+value)
            self.set_ctx_state('username', value)

    def label(self, ident: int, type_: int) -> str:
        """PMDA label"""
        if type_ == c_api.PM_LABEL_CLUSTER:
            if ident == Consts.Control.Cluster:
                pass
            elif ident == Consts.Info.Cluster:
                return f'{{"metrictype":"{MetricType.Control}"}}'
            else:  # script cluster
                return self.clusters[ident].label(ident, type_)
        elif type_ == c_api.PM_LABEL_ITEM:
            cluster = self.pmid_cluster(ident)
            if cluster not in [Consts.Control.Cluster, Consts.Info.Cluster]:
                return self.clusters[cluster].label(ident, type_)
        return '{}'

    def refresh_script_indom(self):
        """update bpftrace scripts indom"""
        self.script_indom.update([cluster.name for cluster in self.clusters.values()])

    def find_cluster_by_name(self, name: str) -> Optional[BPFtraceCluster]:
        """find cluster by name"""
        for cluster in self.clusters.values():
            if cluster.name == name:
                return cluster
        return None

    def register_script(self, code: str):
        """register a new bpftrace script"""
        script = Script(code)
        script.username = self.get_ctx_state('username', default='anonymous')
        script = self.bpftrace_service.register_script(script)
        self.set_ctx_state('register', script)
        if script.state.status == Status.Error:
            return c_api.PM_ERR_BADSTORE

        cluster = BPFtraceCluster(self, self.logger, self.bpftrace_service, script)
        cluster.register_metrics()
        self.clusters[cluster.cluster_id] = cluster
        self.refresh_script_indom()
        return 0

    def deregister_script(self, id_or_name: str):
        """deregister a bpftrace script"""
        cluster = self.find_cluster_by_name(id_or_name)
        if not cluster:
            self.set_ctx_state('deregister', {"error": "script not found"})
            return c_api.PM_ERR_BADSTORE

        cluster.deregister_metrics()
        self.bpftrace_service.deregister_script(cluster.script.script_id)
        del self.clusters[cluster.cluster_id]
        self.refresh_script_indom()
        self.set_ctx_state('deregister', {"success": "true"})
        return 0

    def store_callback(self, cluster: int, item: int, inst: int, val: str):  # pylint: disable=too-many-return-statements
        """PMDA store callback"""
        if cluster == Consts.Control.Cluster:
            username = self.get_ctx_state('username', default='anonymous')
            if username not in self.config.allowed_users:
                return c_api.PM_ERR_PERMISSION

            if item == Consts.Control.Register:
                return self.register_script(val)
            elif item == Consts.Control.Deregister:
                return self.deregister_script(val)
            elif item == Consts.Control.Start:
                self.logger.info("start script again")
                cluster = self.find_cluster_by_name(val)
                if not cluster:
                    self.set_ctx_state('start', {"error": "script not found"})
                    return c_api.PM_ERR_BADSTORE

                self.bpftrace_service.start_script(cluster.script.script_id)
                self.set_ctx_state('start', {"success": "true"})
                return 0
            elif item == Consts.Control.Stop:
                cluster = self.find_cluster_by_name(val)
                if not cluster:
                    self.set_ctx_state('stop', {"error": "script not found"})
                    return c_api.PM_ERR_BADSTORE

                self.bpftrace_service.stop_script(cluster.script.script_id)
                self.set_ctx_state('stop', {"success": "true"})
                return 0
        return c_api.PM_ERR_PMID

    def sync_scripts_with_process_manager(self):
        # expiration timer may have removed idle scripts
        script_ids = self.bpftrace_service.list_scripts()
        if script_ids:
            changed = False
            for cluster in list(self.clusters.values()):
                if cluster.script.script_id not in script_ids:
                    cluster.deregister_metrics()
                    del self.clusters[cluster.cluster_id]
                    changed = True
            if changed:
                self.refresh_script_indom()

    def refresh_callback(self, cluster: int):
        """PMDA refresh callback"""
        self.sync_scripts_with_process_manager()

        if cluster in [Consts.Control.Cluster, Consts.Info.Cluster]:
            pass
        elif cluster in self.clusters:
            self.clusters[cluster].refresh_callback()

    def fetch_callback(self, cluster: int, item: int, inst: int):
        """PMDA fetch callback"""
        if cluster == Consts.Control.Cluster:
            if item == Consts.Control.Register:
                json_val = json.dumps(self.get_ctx_state('register', {}, True), cls=ScriptEncoder)
                return [json_val, 1]
            elif item == Consts.Control.Deregister:
                json_val = json.dumps(self.get_ctx_state('deregister', {}, True))
                return [json_val, 1]
            elif item == Consts.Control.Start:
                json_val = json.dumps(self.get_ctx_state('start', {}, True))
                return [json_val, 1]
            elif item == Consts.Control.Stop:
                json_val = json.dumps(self.get_ctx_state('stop', {}, True))
                return [json_val, 1]
        elif cluster == Consts.Info.Cluster:
            if item == Consts.Info.Scripts:
                id_or_name = self.script_indom.inst_name_lookup(inst)
                if id_or_name is None:
                    return [c_api.PM_ERR_INST, 0]

                cluster = self.find_cluster_by_name(id_or_name)
                if cluster:
                    return [cluster.script.code, 1]
                return [c_api.PM_ERR_INST, 0]
            elif item == Consts.Info.ScriptsJson:
                scripts = [cluster.script for cluster in self.clusters.values()]
                return [json.dumps(scripts, cls=ScriptEncoder), 1]
            elif item == Consts.Info.Tracepoints:
                return [self.tracepoints_csv, 1]
        elif cluster in self.clusters:
            return self.clusters[cluster].fetch_callback(item, inst)
        return [c_api.PM_ERR_PMID, 0]


if __name__ == '__main__':
    BPFtracePMDA('bpftrace', 151).run()
