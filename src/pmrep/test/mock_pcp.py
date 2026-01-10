#!/usr/bin/env python3
#
# Copyright (c) 2025 Red Hat.
#
# Mock PCP modules for unit testing without PCP installed.
#
# This module provides stub implementations of PCP modules (pcp, cpmapi, etc.)
# that are inserted into sys.modules BEFORE importing pmrep. This allows
# unit tests to run without requiring PCP to be installed.
#

import sys
from unittest.mock import MagicMock, Mock
from collections import OrderedDict

# Create mock cpmapi module with constants
mock_cpmapi = MagicMock()
mock_cpmapi.PM_CONTEXT_ARCHIVE = 2
mock_cpmapi.PM_CONTEXT_HOST = 1
mock_cpmapi.PM_CONTEXT_LOCAL = 0
mock_cpmapi.PM_INDOM_NULL = 0xffffffff
mock_cpmapi.PM_IN_NULL = 0xffffffff
mock_cpmapi.PM_TIME_SEC = 1
mock_cpmapi.PM_SEM_DISCRETE = 4
mock_cpmapi.PM_TYPE_STRING = 6
mock_cpmapi.PM_TEXT_PMID = 1
mock_cpmapi.PM_TEXT_INDOM = 2
mock_cpmapi.PM_TEXT_ONELINE = 1
mock_cpmapi.PM_TEXT_HELP = 2
mock_cpmapi.PM_LABEL_INDOM = 4
mock_cpmapi.PM_LABEL_INSTANCES = 8
mock_cpmapi.PM_LABEL_DOMAIN = 1
mock_cpmapi.PM_LABEL_CLUSTER = 2
mock_cpmapi.PM_LABEL_ITEM = 3

# Create mock cpmi module
mock_cpmi = MagicMock()
mock_cpmi.PMI_ERR_DUPINSTNAME = -1001
mock_cpmi.PMI_ERR_DUPTEXT = -1002

# Create mock pmapi module
mock_pmapi = MagicMock()
mock_pmapi.c_api = mock_cpmapi

# Mock timespec class
class MockTimespec:
    def __init__(self, seconds=1):
        self.seconds = seconds
    def __float__(self):
        return float(self.seconds)
    def __str__(self):
        return str(self.seconds)

mock_pmapi.timespec = MockTimespec

# Mock pmErr exception
class MockPmErr(Exception):
    def __init__(self, *args):
        super().__init__(*args)
    def progname(self):
        return "pmrep"
    def message(self):
        return str(self.args[0]) if self.args else "Unknown error"

mock_pmapi.pmErr = MockPmErr

# Mock pmUsageErr exception
class MockPmUsageErr(Exception):
    def message(self):
        pass

mock_pmapi.pmUsageErr = MockPmUsageErr

# Mock pmOptions class
class MockPmOptions:
    def __init__(self):
        self.mode = 0
        self.delta = 1.0
    def pmSetOptionCallback(self, cb): pass
    def pmSetOverrideCallback(self, cb): pass
    def pmSetShortOptions(self, opts): pass
    def pmSetShortUsage(self, usage): pass
    def pmSetLongOptionHeader(self, header): pass
    def pmSetLongOptionArchive(self): pass
    def pmSetLongOptionArchiveFolio(self): pass
    def pmSetLongOptionContainer(self): pass
    def pmSetLongOptionHost(self): pass
    def pmSetLongOptionLocalPMDA(self): pass
    def pmSetLongOptionSpecLocal(self): pass
    def pmSetLongOption(self, *args): pass
    def pmSetLongOptionDebug(self): pass
    def pmSetLongOptionVersion(self): pass
    def pmSetLongOptionHelp(self): pass
    def pmSetLongOptionAlign(self): pass
    def pmSetLongOptionStart(self): pass
    def pmSetLongOptionFinish(self): pass
    def pmSetLongOptionOrigin(self): pass
    def pmSetLongOptionSamples(self): pass
    def pmSetLongOptionInterval(self): pass
    def pmSetLongOptionTimeZone(self): pass
    def pmSetLongOptionHostZone(self): pass
    def pmSetOptionInterval(self, interval): pass
    def pmGetOptionContext(self): return 1
    def pmGetOptionHosts(self): return []
    def pmGetOptionArchives(self): return []
    def pmGetOptionSamples(self): return None
    def pmGetOptionInterval(self): return 1.0
    def pmGetOptionStart(self): return None
    def pmGetOptionFinish(self): return None
    def pmGetOptionOrigin(self): return None
    def pmGetOptionAlignment(self): return None
    def daemonize(self): pass

mock_pmapi.pmOptions = MockPmOptions

# Mock pmContext class
class MockPmContext:
    type = 1
    ctx = 0

    def __init__(self, *args):
        pass

    @staticmethod
    def set_connect_options(*args):
        return (1, "local:")

    @staticmethod
    def fromOptions(*args):
        return MockPmContext()

    @staticmethod
    def pmID_domain(pmid):
        return 0

    @staticmethod
    def pmID_cluster(pmid):
        return 0

    @staticmethod
    def pmID_item(pmid):
        return 0

    def pmDebug(self, flag):
        return False

    def pmGetContextHostName(self):
        return "localhost"

    def get_current_tz(self, opts=None):
        return "UTC"

    def posix_tz_to_utc_offset(self, tz):
        return "+0000"

    def prepare_execute(self, *args):
        pass

    def datetime_to_secs(self, dt, scale):
        return 0.0

    def pmGetArchiveEnd(self):
        return 0.0

    def pmGetArchiveLabel(self):
        mock = MagicMock()
        mock.hostname = "localhost"
        return mock

mock_pmapi.pmContext = MockPmContext

# Mock fetchgroup class
class MockFetchgroup:
    def __init__(self, *args):
        pass

    def get_context(self):
        return MockPmContext()

    def extend_item(self, *args):
        return MagicMock()

    def extend_indom(self, *args):
        return MagicMock()

    def extend_timeval(self):
        from datetime import datetime
        return lambda: datetime.now()

    def extend_timespec(self):
        return lambda: 0

    def fetch(self):
        return 0

    def clear(self):
        pass

mock_pmapi.fetchgroup = MockFetchgroup

# Create mock pmconfig module
mock_pmconfig = MagicMock()
mock_pmconfig.TRUNC = "..."

class MockPmConfig:
    metricspec = ('label', 'unit', 'width', 'precision', 'limit', 'formula')

    def __init__(self, util):
        self.util = util
        self.pmids = []
        self.descs = []
        self.insts = []
        self.texts = []
        self.labels = []
        self.res_labels = {}

    def set_config_path(self, paths):
        return None

    def read_options(self):
        pass

    def read_cmd_line(self):
        pass

    def prepare_metrics(self):
        pass

    def set_signal_handler(self):
        pass

    def validate_common_options(self):
        pass

    def validate_metrics(self, curr_insts=True):
        pass

    def finalize_options(self):
        pass

    def fetch(self):
        return 0

    def pause(self):
        pass

    def get_ranked_results(self, valid_only=True):
        return {}

    def update_metrics(self, curr_insts=True):
        pass

    def parse_instances(self, inst_str):
        return []

    def get_labels_str(self, metric, inst, dynamic, json_fmt):
        return ""

mock_pmconfig.pmConfig = MockPmConfig
mock_pmconfig.TRUNC = "..."

# Create mock pmi module
mock_pmi = MagicMock()

class MockPmiErr(Exception):
    def errno(self):
        return 0

mock_pmi.pmiErr = MockPmiErr
mock_pmi.pmiLogImport = MagicMock

# Create the mock pcp package
mock_pcp = MagicMock()
mock_pcp.pmapi = mock_pmapi
mock_pcp.pmconfig = mock_pmconfig
mock_pcp.pmi = mock_pmi


def install_mocks():
    """Install mock PCP modules into sys.modules"""
    sys.modules['cpmapi'] = mock_cpmapi
    sys.modules['cpmi'] = mock_cpmi
    sys.modules['pcp'] = mock_pcp
    sys.modules['pcp.pmapi'] = mock_pmapi
    sys.modules['pcp.pmconfig'] = mock_pmconfig
    sys.modules['pcp.pmi'] = mock_pmi


def uninstall_mocks():
    """Remove mock PCP modules from sys.modules"""
    for mod in ['cpmapi', 'cpmi', 'pcp', 'pcp.pmapi', 'pcp.pmconfig', 'pcp.pmi']:
        if mod in sys.modules:
            del sys.modules[mod]
