#!/usr/bin/env pmpython
""" Performance Metric Domain Agent for SAP HANA.

Performance Metric Domain Agent for SAP HANA.
https://pcp.io/
https://www.sap.com/products/hana.html

Copyright (c) 2021 Red Hat.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
"""
import argparse
import configparser
import os.path
import sys
from distutils.version import LooseVersion
from typing import Any, Dict, List, Optional

from cpmapi import (
    PM_COUNT_ONE,
    PM_ERR_AGAIN,
    PM_ERR_INST,
    PM_ERR_PMID,
    PM_INDOM_NULL,
    PM_SEM_COUNTER,
    PM_SEM_DISCRETE,
    PM_SEM_INSTANT,
    PM_SPACE_BYTE,
    PM_SPACE_GBYTE,
    PM_TIME_MSEC,
    PM_TIME_SEC,
    PM_TIME_USEC,
    PM_TYPE_32,
    PM_TYPE_64,
    PM_TYPE_DOUBLE,
    PM_TYPE_FLOAT,
    PM_TYPE_STRING,
    PM_TYPE_U32,
    PM_TYPE_U64,
)
from hdbcli import dbapi
from pcp.pmapi import pmContext, pmUnits
from pcp.pmda import PMDA, pmdaIndom, pmdaInstid, pmdaMetric
from pyhdbcli import OperationalError, ResultRow


class HDBConnection:
    """
    Wrapper class for a connection to HANA.
    """

    def __init__(self, address: str, port: int, user: str, password: str):
        # Connection parameters of the driver are documented here
        # https://help.sap.com/viewer/f1b440ded6144a54ada97ff95dac7adf/2.10/en-US/ee592e89dcce4480a99571a4ae7a702f.html
        self._conn = dbapi.connect(
            address=address,
            port=port,
            user=user,
            password=password,
            databaseName=None,
            # Controls whether the system automatically reconnects to the database instance after a command timeout or
            # when the connection is broken. Reconnecting restores the old state.
            reconnect=True,
            # limits the overall time for the entire connection to try all server addresses specified.
            # See nodeConnectTimeout for per-node timeout.
            connectTimeout=60 * 1000,
            # Aborts the connection attempt after the specified maximum timeout
            nodeConnectTimeout=10 * 1000,
            # wait for a maximum of the specified number of milliseconds for any request to complete. If the time has
            # expired and a response has not been received, the driver returns an error.
            communicationTimeout=2 * 1000,
        )

    def query(self, sql: str, parameters: Optional[Dict] = None) -> List[ResultRow]:
        """
        Queries the database and returns a list of rows.
        :param sql: str
            SQL query to execute. The query should select a single column and result in a single (or zero) row result.
        :param parameters:
            Optional dictionary of query parameters.
        :return:
            List of rows.
        """
        if parameters is None:
            parameters = {}
        cursor = self._conn.cursor()
        try:
            cursor.execute(sql, parameters)
        except dbapi.ProgrammingError as ex:
            raise dbapi.ProgrammingError(
                f"ex={ex}, sql='{sql}', paramters={parameters}"
            ) from ex

        result = cursor.fetchall()
        cursor.close()
        return result

    def query_scalar(
        self, sql: str, parameters: Optional[Dict] = None
    ) -> Optional[Any]:
        """
        Queries the database and returns a scalar (one value).
        :param sql: str
            SQL query to execute. The query should select a single column and result in a single (or zero) row result.
        :param parameters:
            Optional dictionary of query parameters.
        :return:
            A single value. None if the query did not return a result.
        """
        result = self.query(sql, parameters)
        if len(result) == 0:
            return None
        if len(result) > 1:
            # programming error (where predicate not precise enough)
            raise RuntimeError(
                f"Query returned {len(result)} rows, expected was one row. query={sql}"
            )
        scalar = result[0].column_values[0]
        return scalar


# HANA 2 Support package stacks (SPS) and revisions
# https://help.sap.com/viewer/42668af650f84f9384a3337bcd373692/2.0.05/en-US
_HANA2_SPS_06 = "2.00.060"
_HANA2_SPS_05 = "2.00.050"
_HANA2_SPS_04 = "2.00.040"
_HANA2_SPS_03 = "2.00.030"
_HANA2_SPS_02 = "2.00.020"
_HANA2_SPS_01 = "2.00.010"
_HANA2_SPS_00 = "2.00.000"


def _hana_revision_included(
    revision: str, min_revision: Optional[str], max_revision: Optional[str]
) -> bool:
    """
    Checks wherever a HANA 2 revision is included in the revision interval [min_revision, max_revision]
    :param revision: str
        revision to check
    :param min_revision: Optional[str]
        lower end of revisions to accept. If no lower end exists, set to None.
    :param max_revision: Optional[str]
        upper end of revisions to accept. If no upper end exists, set to None.
    :return: bool
        True iff revision is within the compatability interval.
    """
    if revision is None:
        raise ValueError("version must not be None")
    v_revision = LooseVersion(revision)
    v_min = LooseVersion(min_revision) if min_revision else None
    v_max = LooseVersion(max_revision) if max_revision else None

    if v_min is not None and v_max is not None:
        if v_max < v_min:
            raise ValueError(
                f"min_version ({min_revision}) is greater than max_version ({max_revision})"
            )

    if v_min is not None:
        if v_revision < v_min:
            return False

    if v_max is not None:
        if v_revision > v_max:
            return False

    return True


class Metric:
    """
    Metric class links the Performance Co-Pilot (PCP, PMDA) domain with HANA.
    """

    # PMDA unit constants
    UNITS_NONE = pmUnits(0, 0, 0, 0, 0, 0)
    UNITS_SECOND = pmUnits(0, 1, 0, 0, PM_TIME_SEC, 0)
    UNITS_MILLISECOND = pmUnits(0, 1, 0, 0, PM_TIME_MSEC, 0)
    UNITS_MICROSECOND = pmUnits(0, 1, 0, 0, PM_TIME_USEC, 0)
    UNITS_COUNT = pmUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)
    UNITS_BYTE = pmUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0)
    UNITS_GIGABYTE = pmUnits(1, 0, 0, PM_SPACE_GBYTE, 0, 0)

    def __init__(
        self,
        name: str,
        desc: str,
        meta: pmdaMetric,
        query: str,
        help_text: Optional[str] = None,
        min_hana_revision: Optional[str] = None,
        max_hana_revision: Optional[str] = None,
    ):
        """
        :param name: str
            Name of the metric. Must be unique.
        :param desc: str
            One line description of the metric.
        :param meta: pmdaMetric
            PCP specific meta data of the metric. See pmdaMetric for details.
        :param query: str
            SQL query that returns the value of the metric. If the Metric has an associated instance domain,
            use a parameterised SQL query.
        :param help_text: str
            Long help text. If not set, @desc will be used.
        :param min_hana_revision: str
            Minimum HANA 2 revision that supports this metric. If the metric has no minimum revision, i.e. it is
            supported right from HANA2 SPS00, set to None.
        :param max_hana_revision: str
            Maximum HANA 2 revision that supports this metric. If the metric has no maximum revision, i.e. it has not
            removed from newer revisions, set to None.
        """
        self.name = name
        self.desc = desc
        self.meta = meta
        self.query = query
        self.help_text = help_text
        self.min_hana_revision = min_hana_revision
        self.max_hana_revision = max_hana_revision

        # If an instance domain is set, the query needs a WHERE clause that filters the results to match just
        # one instance. A missing where clause is an indicator for a programming mistake. Since this mistake may not
        # necessarely result in a runtime error, we end up with a subtle bug. This check prevents these situations.
        if meta.m_desc.indom != PM_INDOM_NULL:
            if " where " not in query.lower():
                raise ValueError(
                    "Metric with defined instance domain must have a WHERE clause in query"
                )

    def __str__(self):
        return f"{self.name} ({self.query})"


class HdbPMDA(PMDA):
    """
    PMDA implementation for SAP HANA Database.
    """

    def __init__(self, hdb: HDBConnection):
        """
        Instantiates the PMDA and registers all metrics.
        During instantiation, the database will be inspected for various runtime characteristics
        (such as revision, schemata, volumes, etc.).
        :param hdb: HDBConnection
            Connection to the HANA Database to monitor.
        """
        super().__init__("hdb", 158, logfile=None, helpfile=None)
        self._hdb = hdb
        hdb_revision = self._hdb.query_scalar("SELECT VERSION FROM SYS.M_DATABASE;")
        if hdb_revision is None:
            raise RuntimeError("Cannot determine HANA revision")
        self._hdb_revision: str = hdb_revision

        # Lookup table that maps pmids (PCP domain) to this to individual Metric objects.
        self._metric_lookup: Dict[int, Metric] = {}  # pmid -> Metric

        # Each instance domain is mapped to a dictionary that contains the key value pairs to select the right instance
        # when querying the database. The key value pairs are used for named parameter binding in the sql query of
        # the metric. The named parameters in the sql query must match the keys of the dictionary.
        # This approach stores the parameter key per item. It would be fair to reduce this memory footprint by storing
        # the keys only once (in a wrapper struct) and keeping only a list of values per instance item.
        # indom -> {instance -> {p:v}}, where {p:v} are the sql parameters for identification
        self._indom_lookup: Dict[int, Dict[int, Dict[str, str]]] = {}

        # counter for automatically enumerating registered instance domains
        self._instance_domain_id_counter = 0

        self._init_metrics()
        self.set_fetch_callback(self.fetch_callback)
        self.set_user(pmContext.pmGetConfig("PCP_USER"))

    def fetch_callback(self, cluster, item, inst):
        """
        PMDA specific implementation of pmdaFetch.
        pmdaFetch is a generic callback used by a PMDA(3) to process a fetch request from pmcd(1).
        The request from pmcd is initiated by a client calling pmFetch(3).
        See https://man7.org/linux/man-pages/man3/pmdaSetFetchCallBack.3.html for details.
        :param cluster: int
            Cluster to fetch.
        :param item: int
            Item to fetch.
        :param inst: int
            ID of the instance to query. If the metric has no associated instance domain, this value is ignored.
        :return: [value: Any, success:int]
            A two valued array. The first value contains the value or an error code such as PM_ERR_PMID or PM_ERR_AGAIN.
            The second value indicates if the operation was successful (1) or failed (0).
        """
        metric_id = PMDA.pmid(cluster, item)
        try:
            metric = self._metric_lookup[metric_id]
        except KeyError:
            return [PM_ERR_PMID, 0]
        indom_id = metric.meta.m_desc.indom
        if indom_id == PM_INDOM_NULL:
            try:
                value = self._hdb.query_scalar(metric.query)
            except OperationalError as ex:
                self.err(
                    f"Failed to query DB for metric (metric={metric}, cluster={cluster}, item={item}, ex={ex})"
                )
                return [PM_ERR_AGAIN, 0]
        else:
            indom = self._indom_lookup[indom_id]
            try:
                identifier_parameters = indom[inst]
            except KeyError:
                return [PM_ERR_INST, 0]
            try:
                value = self._hdb.query_scalar(metric.query, identifier_parameters)
            except OperationalError as ex:
                self.err(
                    f"Failed to query DB for metric (metric={metric}, cluster={cluster}, item={item}, inst={inst}, "
                    f"identifying_parameters={identifier_parameters}, ex={ex})"
                )
                return [PM_ERR_AGAIN, 0]
            except dbapi.Error as ex:
                raise RuntimeError(
                    f"Failed to query DB for metric (metric={metric}, cluster={cluster}, item={item}, inst={inst}, "
                    f"identifying_parameters={identifier_parameters}, ex={ex})"
                ) from ex
        typed_value = _TYPE_FUNCTIONS[metric.meta.m_desc.type](value)
        # TODO: type casting and error handling.
        #       currently None is mapped to the zero value of the appropriate type
        #       it might make more sense to return PM_ERR_AGAIN instead
        return [typed_value, 1]

    def _init_metrics(self):
        # builders are expected to be of type cluster_id:int -> List[Metric]
        metrics_builders = [
            self._metrics_system_overview,
            self._metrics_schema,
            self._metrics_connections,
            self._metrics_volumes,
            self._metrics_service_memory,
            self._metrics_admission_control,
            self._metrics_buffer_cache,
            self._metrics_buffer_cache_pool,
            self._metrics_column_store_record_locks,
            self._metrics_service_misc,
            self._metrics_volume_io,
            self._metrics_backup,
            self._metrics_caches,
            self._metrics_metadata_locks,
            self._metrics_table_locks,
            self._metrics_transactions,
            self._metrics_statements,
            self._metrics_plan_cache,
            self._metrics_column_unloads,
            self._metrics_mvcc,
            self._metrics_aggregated_host_memory,
            self._metrics_alerts,
            self._metrics_aggregated_host_cpu,
            self._metrics_aggregated_host_io,
        ]
        # TODO: Domains that should be added as well:
        #   - SDA Monitoring via M_REMOTE_CONNECTIONS and M_REMOTE_STATEMENTS
        #   - System replication via M_SYSTEM_REPLICATION
        #   - RowStore memory and table statistics

        for (cluster_index, builder) in enumerate(metrics_builders):
            metrics = builder(cluster_index)
            # Register all metrics with the PMDA and populate the internal lookup table.
            # If any ID is duplicated, the register process fails
            for metric in metrics:
                if not _hana_revision_included(
                    self._hdb_revision,
                    min_revision=metric.min_hana_revision,
                    max_revision=metric.max_hana_revision,
                ):
                    self.log(
                        f"Skipping metric due to incompatible version (metric={metric.name}, "
                        f"hdb_revision={self._hdb_revision}, "
                        f"min_hana_revision={metric.min_hana_revision}, "
                        f"max_hana_revision={metric.max_hana_revision})"
                    )
                    continue
                self.add_metric(
                    metric.name,
                    metric.meta,
                    oneline=metric.desc,
                    text=metric.help_text
                    if metric.help_text is not None
                    else metric.desc,
                )
                self._metric_lookup[metric.meta.m_desc.pmid] = metric

    def _build_instance_domain(
        self,
        enum_query: str,
        min_hana_revision: Optional[str] = None,
        max_hana_revision: Optional[str] = None,
    ) -> int:
        # check version compat
        if not _hana_revision_included(
            self._hdb_revision, min_hana_revision, max_hana_revision
        ):
            self.log(
                f"Skipping instance domain due to incompatible version (enum_query={enum_query}, hdb_revision={self._hdb_revision}, min_hana_revision={min_hana_revision}, max_hana_revision={max_hana_revision})"
            )
            return PM_INDOM_NULL

        # generate new global id
        domain_id = self._instance_domain_id_counter
        self._instance_domain_id_counter += 1
        indom = self.indom(domain_id)

        # read all instances
        instances = []
        parameters = {}
        instance_rows = self._hdb.query(enum_query)
        for (i, row) in enumerate(instance_rows):
            # string representation that will identify the instance
            id_string = ".".join(map(str, row.column_values))
            instance = pmdaInstid(i, id_string)
            instances.append(instance)
            # store instance value dictionary such that they can be used in the sql statements later on
            parameters[i] = dict(zip(row.column_names, row.column_values))
        try:
            self.add_indom(pmdaIndom(indom, instances))
        except KeyError as ex:
            raise RuntimeError(
                f"Duplicate instance domain id (domain_id={domain_id})"
            ) from ex

        self._indom_lookup[indom] = parameters
        return indom

    def _metrics_table_locks(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT"
        )
        return [
            Metric(
                "hdb.table_locks.total_waits_count",
                "Accumulated lock wait count for table locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_LOCK_WAITS FROM SYS.M_LOCK_WAITS_STATISTICS WHERE LOCK_TYPE='TABLE' AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.table_locks.total_wait_time_microseconds",
                "Accumulated lock wait time (in microseconds) for table locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_LOCK_WAIT_TIME FROM SYS.M_LOCK_WAITS_STATISTICS WHERE LOCK_TYPE='TABLE' AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    @staticmethod
    def _metrics_system_overview(cluster_id: int) -> List[Metric]:
        return [
            Metric(
                "hdb.instance_id",
                "SAP instance ID",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_STRING,
                    PM_INDOM_NULL,
                    PM_SEM_DISCRETE,
                    Metric.UNITS_NONE,
                ),
                "SELECT SYSTEM_ID FROM SYS.M_DATABASE",
            ),
            Metric(
                "hdb.instance_number",
                "Instance number",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U32,
                    PM_INDOM_NULL,
                    PM_SEM_DISCRETE,
                    Metric.UNITS_NONE,
                ),
                "SELECT value FROM SYS.M_SYSTEM_OVERVIEW WHERE section='System' AND name='Instance Number'",
            ),
            Metric(
                "hdb.version",
                "HANA Version (Revision)",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_STRING,
                    PM_INDOM_NULL,
                    PM_SEM_DISCRETE,
                    Metric.UNITS_NONE,
                ),
                "SELECT VERSION FROM SYS.M_DATABASE",
            ),
        ]

    def _metrics_transactions(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )

        return [
            Metric(
                "hdb.transactions.active_count",
                "Current number of active transactions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='ACTIVE'",
            ),
            Metric(
                "hdb.transactions.inactive_count",
                "Current number of inactive transactions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='INACTIVE'",
            ),
            Metric(
                "hdb.transactions.precomitted_count",
                "Current number of precommitted transactions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='PRECOMMITTED'",
            ),
            Metric(
                "hdb.transactions.aborting_count",
                "Current number of aborting transactions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='ABORTING'",
            ),
            Metric(
                "hdb.transactions.partial_aborting_count",
                "Current number of transactions in partial aborting status",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='PARTIAL_ABORTING'",
            ),
            Metric(
                "hdb.transactions.active_prepare_count",
                "Current number of transactions in active prepare status",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND TRANSACTION_STATUS='ACTIVE_PREPARE_COMMIT'",
            ),
            Metric(
                "hdb.transactions.update_count",
                "Number of update transactions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT UPDATE_TRANSACTION_COUNT FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.commit_count",
                "Number of transaction commits",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COMMIT_COUNT FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.rollback_count",
                "Number of transaction rollbacks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ROLLBACK_COUNT FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.update_rate",
                "Current update transaction count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_UPDATE_TRANSACTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.peak_update_rate",
                "Peak update transaction count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_UPDATE_TRANSACTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.transaction_rate",
                "Current transaction count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_TRANSACTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.peak_transaction_rate",
                "Peak transaction count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_TRANSACTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.commit_rate",
                "Current number of commits per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 13),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_COMMIT_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.peak_commit_rate",
                "Peak number of commits per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 14),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_COMMIT_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.peak_rollbacks_rate",
                "Current number of rollbacks per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 15),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_ROLLBACK_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.peak_rollback_rate",
                "Peak rollback of rollbacks per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 16),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_ROLLBACK_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.blocked_count",
                "Number of currently blocked transactions waiting for locks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 17),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BLOCKED_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.transactions.blocked_record_count",
                "Number of currently blocked transactions waiting for record locks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 18),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BLOCKED_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND LOCK_TYPE='RECORD'",
            ),
            Metric(
                "hdb.transactions.blocked_table_count",
                "Number of currently blocked transactions waiting for table locks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 19),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BLOCKED_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND LOCK_TYPE='TABLE'",
            ),
            Metric(
                "hdb.transactions.blocked_metadata_count",
                "Number of currently blocked transactions waiting for metadata locks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 20),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BLOCKED_TRANSACTIONS WHERE HOST=:HOST AND PORT=:PORT AND LOCK_TYPE='METADATA'",
            ),
        ]

    def _metrics_statements(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )
        return [
            Metric(
                "hdb.statements.execution_count",
                "Total count of all executed statements for data manipulation, data definition, and system control",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT EXECUTION_COUNT FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.statements.execution_rate",
                "Current statement execution count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_EXECUTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.statements.peak_execution_rate",
                "peak statement execution count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_EXECUTION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.statements.compilation_rate",
                "Current statement preparation count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT EXECUTION_COUNT FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.statements.peak_compilation_rate",
                "Peak statement preparation count per minute",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_DOUBLE,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PEAK_COMPILATION_RATE FROM SYS.M_WORKLOAD WHERE HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_plan_cache(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )

        return [
            Metric(
                "hdb.plan_cache.capacity_bytes",
                "SQL Plan Cache capacity in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT PLAN_CACHE_CAPACITY FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.size_bytes",
                "SQL Plan Cache size in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT CACHED_PLAN_SIZE FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.fill_ratio_percent",
                "SQL Plan Cache fill ratio in percent",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_FLOAT,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT TO_DECIMAL((CACHED_PLAN_SIZE / NULLIF(PLAN_CACHE_CAPACITY, 0) * 100), 10, 2) FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.hit_ratio",
                "SQL Plan Cache hit ratio",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_FLOAT,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT PLAN_CACHE_HIT_RATIO FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.lookups_count",
                "Nmber of plan lookup counts from SQL Plan Cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PLAN_CACHE_LOOKUP_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.hits_count",
                "Number of hit counts from SQL Plan Cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PLAN_CACHE_HIT_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.avg_cache_time_microseconds",
                "Average duration in microseconds between plan cache insertion and eviction",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT EVICTED_PLAN_AVG_CACHE_TIME FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.count",
                "Number of evicted plans from SQL Plan Cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT EVICTED_PLAN_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.preparation_count",
                "Number of total plan preparations for evicted plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT EVICTED_PLAN_PREPARATION_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.execution_count",
                "Number of total plan executions for evicted plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT EVICTED_PLAN_EXECUTION_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.total_preparation_time_microseconds",
                "Total duration in microseconds for plan preparation for all evicted plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT EVICTED_PLAN_PREPARATION_TIME FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.total_cursor_duration_microseconds",
                "Total cursor duration in microseconds for evicted plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT EVICTED_PLAN_CURSOR_DURATION FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.total_execution_time_microseconds",
                "Total execution time in microseconds for evicted plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT EVICTED_PLAN_TOTAL_EXECUTION_TIME FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.evicted_plans.size_bytes",
                "Accumulated total size of evicted plans in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 13),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_BYTE,
                ),
                "SELECT EVICTED_PLAN_SIZE FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.count",
                "Total number of cached plans in SQL Plan Cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 14),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CACHED_PLAN_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.preparation_count",
                "Total number of plan preparations for cached plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 15),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CACHED_PLAN_PREPARATION_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.execution_count",
                "Number of total plan executions for cached plans",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 16),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CACHED_PLAN_EXECUTION_COUNT FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.total_preparation_time_microseconds",
                "Total plan preparation duration for cached plans in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 17),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT CACHED_PLAN_PREPARATION_TIME FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.total_cursor_duration_microseconds",
                "Total cursor duration for cached plans in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 18),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT CACHED_PLAN_CURSOR_DURATION FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.plan_cache.cached_plans.total_execution_time_microseconds",
                "Total execution time for cached plans in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 19),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT CACHED_PLAN_TOTAL_EXECUTION_TIME FROM SYS.M_SQL_PLAN_CACHE_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_column_unloads(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )
        return [
            Metric(
                "hdb.column_unloads.low_memory_count",
                "Number of column unloads caused by memory management's automatic shrink on out of memory (OOM)",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_CS_UNLOADS WHERE HOST=:HOST AND PORT=:PORT AND REASON='LOW MEMORY'",
            ),
            Metric(
                "hdb.column_unloads.shrink_count",
                "Number of column unloads caused by a manual shrink",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_CS_UNLOADS WHERE HOST=:HOST AND PORT=:PORT AND REASON='SHRINK'",
            ),
            Metric(
                "hdb.column_unloads.explicit_count",
                "Number of explicit column unloads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_CS_UNLOADS WHERE HOST=:HOST AND PORT=:PORT AND REASON='EXPLICIT'",
            ),
            Metric(
                "hdb.column_unloads.merge_count",
                "Number of column unloads due to merges",
                pmdaMetric(
                    PMDA.pmid(18, 3),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_CS_UNLOADS WHERE HOST=:HOST AND PORT=:PORT AND REASON='MERGE'",
            ),
        ]

    def _metrics_alerts(self, cluster_id: int) -> List[Metric]:
        indom = self._build_instance_domain(
            "SELECT S.HOST AS HOST, S.PORT AS PORT, R.ALERT_RATING AS ALERT_RATING FROM SYS.M_SERVICES AS S FULL OUTER JOIN (SELECT ELEMENT_NUMBER AS ALERT_RATING FROM SERIES_GENERATE_INTEGER(1, 0, 5)) as R ON (1=1) WHERE S.SERVICE_NAME<>'daemon' ORDER BY S.HOST,S.PORT,R.ALERT_RATING;"
        )
        return [
            Metric(
                "hdb.alerts.active_count",
                "Number of currently active alerts reported by the statistics server per rating",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_32,
                    indom,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM _SYS_STATISTICS.STATISTICS_CURRENT_ALERTS WHERE ALERT_HOST=:HOST AND ALERT_PORT=:PORT AND ALERT_RATING=:ALERT_RATING",
            ),
        ]

    def _metrics_metadata_locks(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )
        return [
            Metric(
                "hdb.metadata_locks.total_waits_count",
                "Accumulated lock wait count for metadata locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_LOCK_WAITS FROM SYS.M_LOCK_WAITS_STATISTICS WHERE LOCK_TYPE='METADATA' AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.metadata_locks.total_wait_time_microseconds",
                "Accumulated lock wait time (in microseconds) for metadata locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_LOCK_WAIT_TIME FROM SYS.M_LOCK_WAITS_STATISTICS WHERE LOCK_TYPE='METADATA' AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_caches(self, cluster_id: int) -> List[Metric]:
        indom_caches = self._build_instance_domain(
            "SELECT HOST, PORT, VOLUME_ID, CACHE_ID FROM SYS.M_CACHES ORDER BY HOST,PORT,VOLUME_ID,CACHE_ID;"
        )
        return [
            Metric(
                "hdb.cache.total_size_bytes",
                "Maximum available memory budget in bytes available for the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_64,
                    indom_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT TOTAL_SIZE FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.used_size_bytes",
                "Memory in bytes used by the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_64,
                    indom_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_SIZE FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.entries_count",
                "Number of entries in the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ENTRY_COUNT FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.inserts_count",
                "Number of insertions into the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_caches,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT INSERT_COUNT FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.invalidations_count",
                "Number of invalidations in the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_caches,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT INVALIDATE_COUNT FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.hits_count",
                "Number of cache hits for the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_caches,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT HIT_COUNT FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.cache.miss_count",
                "Number of cache misses for the cache instance",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_caches,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT MISS_COUNT FROM SYS.M_CACHES WHERE CACHE_ID=:CACHE_ID AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_volume_io(self, cluster_id: int) -> List[Metric]:
        indom_volumes = self._build_instance_domain(
            "SELECT HOST, PORT, VOLUME_ID, TYPE FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS ORDER BY HOST,PORT,VOLUME_ID,TYPE;"
        )
        return [
            #   This cluster contains aggregated info across all buffers.
            #   For detailed I/O statistics for various buffer sizes see M_VOLUME_IO_DETAILED_STATISTICS
            Metric(
                "hdb.volumes.io.blocked_write_requests_count",
                "Number of blocked write requests",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT BLOCKED_WRITE_REQUESTS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.max_blocked_write_requests_count",
                "Maximum number of blocked write requests",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT MAX_BLOCKED_WRITE_REQUESTS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_reads_count",
                "Total number of synchronous reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_READS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_async_reads_count",
                "Total number of triggered asynchronous reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_TRIGGER_ASYNC_READS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.active_async_reads_count",
                "Number of active asynchronous reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACTIVE_ASYNC_READS_COUNT FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.async_reads_trigger_ratio",
                "Trigger-ratio of asynchronous reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_DOUBLE,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT TRIGGER_READ_RATIO FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_short_reads_count",
                "Total number of reads that read fewer bytes than requested",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_SHORT_READS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_full_retry_reads_count",
                "Total number of full retry reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_FULL_RETRY_READS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_failed_reads_count",
                "Total number of failed reads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_FAILED_READS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_read_bytes",
                "Total number of read data in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_BYTE,
                ),
                "SELECT TOTAL_READ_SIZE FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_read_time_microseconds",
                "Total read time in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_READ_TIME FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_appends_count",
                "Total number of appends",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_APPENDS FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_writes_count",
                "Total number of appends",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_WRITES FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_async_writes_count",
                "Total number of triggered asynchronous writes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 13),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_TRIGGER_ASYNC_WRITES FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.active_async_writes_count",
                "Number of active asynchronous writes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 14),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACTIVE_ASYNC_WRITES_COUNT FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.async_writes_trigger_ratio",
                "Trigger-ratio of asynchronous writes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 15),
                    PM_TYPE_DOUBLE,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT TRIGGER_WRITE_RATIO FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_short_writes_count",
                "Total number of writes that wrote fewer bytes than requested",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 16),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_SHORT_WRITES FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_full_retry_writes_count",
                "Total number of full retry writes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 17),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_FULL_RETRY_WRITES FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_failed_writes_count",
                "Total number of failed writes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 18),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_FAILED_WRITES FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_written_bytes",
                "Total number of written data in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 19),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_BYTE,
                ),
                "SELECT TOTAL_WRITE_SIZE FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_write_time_microseconds",
                "Total write time in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 20),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_WRITE_TIME FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
            Metric(
                "hdb.volumes.io.total_time_microseconds",
                "Total I/O time in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 21),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_IO_TIME FROM SYS.M_VOLUME_IO_TOTAL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND TYPE=:TYPE",
            ),
        ]

    @staticmethod
    def _metrics_backup(cluster_id: int) -> List[Metric]:
        return [
            Metric(
                "hdb.backup.total_successful_count",
                "Total number of successful backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='successful'",
            ),
            Metric(
                "hdb.backup.total_failed_count",
                "Total number of failed backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='failed'",
            ),
            Metric(
                "hdb.backup.running_count",
                "Number of currently running backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='running'",
            ),
            Metric(
                "hdb.backup.cancel_pending_count",
                "Number of currently running backups with pending cancellation",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='cancel pending'",
            ),
            Metric(
                "hdb.backup.total_canceled_count",
                "Total number of canceled backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='canceled'",
            ),
            Metric(
                "hdb.backup.max_successful_runtime_seconds",
                "Maximum runtime of successful backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_INSTANT,
                    Metric.UNITS_SECOND,
                ),
                "SELECT MAX(SECONDS_BETWEEN(UTC_START_TIME, UTC_END_TIME)) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME='successful'",
            ),
            Metric(
                "hdb.backup.max_current_runtime_seconds",
                "Maximum runtime of currently running (state=running or cancel pending) backups",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    PM_INDOM_NULL,
                    PM_SEM_INSTANT,
                    Metric.UNITS_SECOND,
                ),
                "SELECT MAX(SECONDS_BETWEEN(UTC_START_TIME, NOW())) FROM SYS.M_BACKUP_CATALOG WHERE STATE_NAME IN ('running', 'cancel pending')",
            ),
        ]

    def _metrics_schema(self, cluster_id: int) -> List[Metric]:
        indom_schemas = self._build_instance_domain(
            "SELECT HOST,PORT,SCHEMA_NAME from SYS.M_CS_TABLES GROUP BY HOST,PORT,SCHEMA_NAME"
        )
        return [
            Metric(
                "hdb.schemas.memory.used_bytes",
                "Total used (column store) memory by schema in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_schemas,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                # Shows run time data per partition (the PART_ID value is the sequential number of the partition).
                # Be aware that after a split/merge operation the memory size is not estimated and therefore the values show zero.
                # A delta merge is required to update the values.
                "SELECT SUM(memory_size_in_total) FROM sys.m_cs_tables WHERE SCHEMA_NAME=:SCHEMA_NAME AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.schemas.total_read_count",
                "Total number of read accesses on the schema across all tables and partitions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_schemas,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                # This is not the number of SELECT statements against this table. A SELECT statement may involve several read accesses.
                "SELECT SUM(READ_COUNT) FROM sys.m_cs_tables WHERE SCHEMA_NAME=:SCHEMA_NAME AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.schemas.total_write_count",
                "Total number of write accesses on the schema across all tables and partitions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_schemas,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                # This is not the number of DML and DDL statements against this table. A DML or DDL statement may involve several write accesses.
                "SELECT SUM(WRITE_COUNT) FROM sys.m_cs_tables WHERE SCHEMA_NAME=:SCHEMA_NAME AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.schemas.total_merge_count",
                "Total number of delta merges on the schema across all tables and partitions",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_schemas,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                # This is not the number of DML and DDL statements against this table. A DML or DDL statement may involve several write accesses.
                "SELECT SUM(MERGE_COUNT) FROM sys.m_cs_tables WHERE SCHEMA_NAME=:SCHEMA_NAME AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_connections(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT"
        )
        return [
            Metric(
                "hdb.connections.running_count",
                "Total number of connections where a statement is being executed",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) num_conns FROM SYS.M_CONNECTIONS WHERE connection_status='RUNNING' AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.connections.idle_count",
                "Total number of connections where no statement is being executed",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) num_conns FROM SYS.M_CONNECTIONS WHERE connection_status='IDLE' AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.connections.queuing_count",
                "Total number of queued connections",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) num_conns FROM SYS.M_CONNECTIONS WHERE connection_status='RUNNING' AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_volumes(self, cluster_id: int) -> List[Metric]:
        # TODO: what about type DATA_BACKUP, LOG_BACKUP, and CATALOG_BACKUP types?
        indom_volumes = self._build_instance_domain(
            "SELECT HOST, PORT, VOLUME_ID FROM SYS.M_VOLUMES ORDER BY VOLUME_ID;"
        )
        return [
            Metric(
                "hdb.volumes.data.used_size_bytes",
                "Size of used and shadow pages in the data volume files in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT SUM(USED_SIZE) FROM SYS.M_VOLUME_FILES WHERE FILE_TYPE='DATA' AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.volumes.data.total_size_bytes",
                "Size of data volumes as reported by the file system in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT SUM(TOTAL_SIZE) FROM SYS.M_VOLUME_FILES WHERE FILE_TYPE='DATA' AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.volumes.log.total_size_bytes",
                "Size of log volumes as reported by the file system in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT SUM(TOTAL_SIZE) FROM SYS.M_VOLUME_FILES WHERE FILE_TYPE='LOG' AND VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.volumes.used_size_bytes",
                "Used size of the data volume in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_SIZE FROM SYS.M_DATA_VOLUME_STATISTICS WHERE VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.volumes.total_size_bytes",
                "Total size of the data volume in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT TOTAL_SIZE FROM SYS.M_DATA_VOLUME_STATISTICS WHERE VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.volumes.fill_ratio",
                "Displays the fill ratio of the data volume",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_FLOAT,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT FILL_RATIO FROM SYS.M_DATA_VOLUME_STATISTICS WHERE VOLUME_ID=:VOLUME_ID AND HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_service_memory(self, cluster_id: int) -> List[Metric]:
        indom_services = self._build_instance_domain(
            # exclude 'daemon' service as it has no meaningful metrics attached (most values are -1)
            "SELECT  HOST, PORT, SERVICE_NAME FROM SYS.M_SERVICES WHERE SERVICE_NAME<>'daemon' ORDER BY HOST,PORT,SERVICE_NAME;"
        )
        return [
            Metric(
                "hdb.services.memory.logical_size_bytes",
                "Virtual memory size from the operating system perspective in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT LOGICAL_MEMORY_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.physical_size_bytes",
                "Physical memory size from the operating system perspective in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT PHYSICAL_MEMORY_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.code_size_bytes",
                "Code size, including shared libraries, in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT CODE_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.stack_size_bytes",
                "Stack size in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT STACK_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.heap_allocated_size_bytes",
                "Heap memory allocated from the memory pool in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT HEAP_MEMORY_ALLOCATED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.heap_used_size_bytes",
                "Heap memory used from the memory pool in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT HEAP_MEMORY_USED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.heap_used_size_percent",
                "Heap memory used in percent",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT TO_DECIMAL((HEAP_MEMORY_USED_SIZE / NULLIF(HEAP_MEMORY_ALLOCATED_SIZE,0) * 100), 10, 2) FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.shared_allocated_size_bytes",
                "Shared memory allocated from the memory pool in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT SHARED_MEMORY_ALLOCATED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.shared_used_size_percent",
                "Shared memory used in percent",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT TO_DECIMAL((SHARED_MEMORY_USED_SIZE / NULLIF(SHARED_MEMORY_ALLOCATED_SIZE,0) * 100), 10, 2) FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.compactors_allocated_size_bytes",
                "Part of the memory pool that can potentially (if unpinned) be freed during a memory shortage in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT COMPACTORS_ALLOCATED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.compactors_freeable_size_bytes",
                "Part of the memory pool that can be freed during a memory shortage in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT COMPACTORS_FREEABLE_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.effective_allocation_limit_size_bytes",
                "Effective maximum memory pool size, in bytes, considering the pool sizes of other processes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT EFFECTIVE_ALLOCATION_LIMIT FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.guaranteed_size_bytes",
                "Minimum guaranteed memory for the process in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT BLOCKED_MEMORY_LIMIT FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.allocated_free_size_bytes",
                "Allocated free memory in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 13),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT FREE_MEMORY_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.virtual_address_space_used_size_bytes",
                "Used size of the virtual address space in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 14),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT VIRTUAL_ADDRESS_SPACE_USED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.virtual_address_space_total_size_bytes",
                "Total size of the virtual address space in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 15),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT VIRTUAL_ADDRESS_SPACE_TOTAL_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.fragmented_size_bytes",
                "Memory held by SAP HANA's memory management that cannot be easily reused for new memory allocations in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 16),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT FRAGMENTED_MEMORY_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.memory.used_size_bytes",
                "Memory in use from the memory pool in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 17),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT TOTAL_MEMORY_USED_SIZE FROM SYS.M_SERVICE_MEMORY WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
        ]

    def _metrics_admission_control(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT",
        )
        return [
            Metric(
                "hdb.admission_control.total_admit_count",
                "Accumulated request admission count",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_ADMIT_COUNT FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.total_reject_count",
                "Accumulated request rejection count",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_REJECT_COUNT FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.total_enqueue_count",
                "Accumulated request enqueued count",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_ENQUEUE_COUNT FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.total_dequeue_count",
                "Accumulated request dequeued count (the executed request count)",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_DEQUEUE_COUNT FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.total_timeout_count",
                "Accumulated request dequeued count due to timeout (the rejected request count)",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_TIMEOUT_COUNT FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.queue_size",
                "Current waiting request queued count",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT CURRENT_QUEUE_SIZE FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.wait_time.last_microseconds",
                "Last wait time of the request in the queue in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT LAST_WAIT_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.wait_time.avg_microseconds",
                "Average wait time of the request in the queue in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT AVG_WAIT_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.wait_time.max_microseconds",
                "Maximum wait time of the request in the queue in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT MAX_WAIT_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.wait_time.min_microseconds",
                "Minimum wait time of the request in the queue in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT MIN_WAIT_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.wait_time.sum_microseconds",
                "Total wait time of the request in the queue in microseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT SUM_WAIT_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.measurement.memory_size_gigabytes",
                "Last measured memory size in GB",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_GIGABYTE,
                ),
                "SELECT LAST_MEMORY_SIZE FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.measurement.memory_allocation_percent",
                "Last measured memory size, as a percentage of the global allocation limit",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT LAST_MEMORY_RATIO FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.admission_control.measurement.timestamp",
                "The time at which the last memory size was measured",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 13),
                    PM_TYPE_STRING,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT LAST_MEMORY_MEASURE_TIME FROM SYS.M_ADMISSION_CONTROL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_buffer_cache(self, cluster_id: int) -> List[Metric]:
        # Assumption is that there is only one NSE buffer cache per volume (named CS).
        indom_volumes = self._build_instance_domain(
            "SELECT  HOST, PORT, VOLUME_ID FROM SYS.M_VOLUMES ORDER BY HOST,PORT,VOLUME_ID;"
        )
        return [
            Metric(
                "hdb.buffer_cache.max_size_bytes",
                "Maximum buffer cache memory capacity in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT MAX_SIZE FROM SYS.M_BUFFER_CACHE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache.allocated_size_bytes",
                "Allocated memory for the buffer cache in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT ALLOCATED_SIZE FROM SYS.M_BUFFER_CACHE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache.used_size_bytes",
                "Used memory for the buffer cache in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_SIZE FROM SYS.M_BUFFER_CACHE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache.reuse_count",
                "Number of times that a buffer is released for reuse by the cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_volumes,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT BUFFER_REUSE_COUNT FROM SYS.M_BUFFER_CACHE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID",
            ),
            Metric(
                "hdb.buffer_cache.hit_ratio",
                "Ratio of pages found in the buffer cache to pages requested from the buffer cache",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_FLOAT,
                    indom_volumes,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT HIT_RATIO FROM SYS.M_BUFFER_CACHE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
        ]

    def _metrics_service_misc(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT",
        )
        indom_services = self._build_instance_domain(
            # exclude 'daemon' service as it has no meaningful metrics attached (most values are -1)
            "SELECT HOST, PORT, SERVICE_NAME FROM SYS.M_SERVICES WHERE SERVICE_NAME<>'daemon' ORDER BY PORT,SERVICE_NAME;"
        )
        return [
            Metric(
                "hdb.services.threads_total_count",
                "Number of total threads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT THREAD_COUNT FROM SYS.M_SERVICE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.threads_active_count",
                "Number of active threads",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACTIVE_THREAD_COUNT FROM SYS.M_SERVICE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.open_files_count",
                "Number of open files",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT OPEN_FILE_COUNT FROM SYS.M_SERVICE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.cpu_percent",
                "CPU usage percentage of the current process",
                # PROCESS_CPU and TOTAL_CPU contain CPU usage in percent since last select.
                # This select could be done by another user or another session.
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_32,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT PROCESS_CPU FROM SYS.M_SERVICE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.cpu_time_milliseconds",
                "CPU usage of the current process since the start in milliseconds as if there would be 1 core",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_services,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT PROCESS_CPU_TIME FROM SYS.M_SERVICE_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SERVICE_NAME=:SERVICE_NAME",
            ),
            Metric(
                "hdb.services.sql_executor_threads_active_count",
                "Number of active SQL Executor threads per indexserver",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U32,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_SERVICE_THREADS WHERE HOST=:HOST AND PORT=:PORT AND THREAD_TYPE='SqlExecutor' AND IS_ACTIVE='TRUE'",
            ),
            Metric(
                "hdb.services.sql_executor_threads_inactive_count",
                "Number of inactive SQL Executor threads per indexserver",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U32,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_SERVICE_THREADS WHERE HOST=:HOST AND PORT=:PORT AND THREAD_TYPE='SqlExecutor' AND IS_ACTIVE='FALSE'",
            ),
        ]

    def _metrics_buffer_cache_pool(self, cluster_id: int) -> List[Metric]:
        indom_buffer_caches = self._build_instance_domain(
            "SELECT HOST, PORT, VOLUME_ID, CACHE_NAME, BUFFER_SIZE FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS ORDER BY HOST,PORT,VOLUME_ID,CACHE_NAME,BUFFER_SIZE",
            min_hana_revision=_HANA2_SPS_04,
        )
        return [
            Metric(
                "hdb.buffer_cache_pool.growth_percent",
                "Rate, as a percentage, at which the buffer pool can grow",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_NONE,
                ),
                "SELECT GROWTH_PERCENT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.total_buffers_count",
                "Number of buffers allocated to the pool",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_BUFFER_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.free_buffers_count",
                "Number of free buffers for the pool",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT FREE_BUFFER_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.lru_list_buffers_count",
                "Number of buffers in the LRU chain for the pool",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT LRU_LIST_BUFFER_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.hot_list_buffers_count",
                "Number of buffers in the hot buffer list for the pool",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT HOT_BUFFER_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.buffers_reuse_count",
                "Number of buffers released from the LRU list for the pool so that a requested page can be cached",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT BUFFER_REUSE_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.buffer_cache_pool.out_of_buffer_event_count",
                "Number number of times that an out-of-buffer situation occurred while requesting buffers from the pool.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_buffer_caches,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT OUT_OF_BUFFER_COUNT FROM SYS.M_BUFFER_CACHE_POOL_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND VOLUME_ID=:VOLUME_ID AND CACHE_NAME=:CACHE_NAME AND BUFFER_SIZE=:BUFFER_SIZE",
                min_hana_revision=_HANA2_SPS_04,
            ),
        ]

    def _metrics_aggregated_host_memory(self, cluster_id: int) -> List[Metric]:
        indom_hosts = self._build_instance_domain(
            "SELECT DISTINCT(HOST) AS HOST FROM SYS.M_SERVICES",
        )
        return [
            Metric(
                "hdb.memory.host_free_bytes",
                "Free physical memory on the host in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT FREE_PHYSICAL_MEMORY FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.host_used_bytes",
                "Used physical memory on the host in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_PHYSICAL_MEMORY FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.swap_free_bytes",
                "Free swap memory on the host in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT FREE_SWAP_SPACE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.swap_used_bytes",
                "Used swap memory on the host in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_SWAP_SPACE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.host_allocation_limit_bytes",
                "Allocation limit for all processes in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT ALLOCATION_LIMIT FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.used_bytes",
                "Amount of memory from the memory pool that is currently being used by SAP HANA processes in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT INSTANCE_TOTAL_MEMORY_USED_SIZE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.peak_used_bytes",
                "Peak memory from the memory pool used by SAP HANA processes since the instance started (this is a sample-based value) in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 6),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT INSTANCE_TOTAL_MEMORY_PEAK_USED_SIZE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.allocated_bytes",
                "Size of the memory pool for all SAP HANA processes in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 7),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT INSTANCE_TOTAL_MEMORY_ALLOCATED_SIZE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.code_size_bytes",
                "Code size, including shared libraries of SAP HANA processes in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 8),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT INSTANCE_CODE_SIZE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.shared_size_bytes",
                "Shared memory size of SAP HANA processes in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 9),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT INSTANCE_SHARED_MEMORY_ALLOCATED_SIZE FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.memory.oom_events.global_allocation_limit_count",
                "Number of out-of-memory (OOM) events since last reset cause by the global allocation limit.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 10),
                    PM_TYPE_32,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_OUT_OF_MEMORY_EVENTS WHERE EVENT_REASON='GLOBAL ALLOCATION LIMIT' AND HOST=:HOST",
            ),
            Metric(
                "hdb.memory.oom_events.process_allocation_limit_count",
                "Number of out-of-memory (OOM) events since last reset caused by a process allocation limit.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 11),
                    PM_TYPE_32,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_OUT_OF_MEMORY_EVENTS WHERE EVENT_REASON='PROCESS ALLOCATION LIMIT' AND HOST=:HOST",
            ),
            Metric(
                "hdb.memory.oom_events.statement_memory_limit_count",
                "Number of out-of-memory (OOM) events since last reset caused by a statement memory limit.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 12),
                    PM_TYPE_32,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT COUNT(1) FROM SYS.M_OUT_OF_MEMORY_EVENTS WHERE EVENT_REASON='STATEMENT MEMORY LIMIT' AND HOST=:HOST",
            ),
        ]

    def _metrics_aggregated_host_io(self, cluster_id: int) -> List[Metric]:
        indom_hosts = self._build_instance_domain(
            "SELECT DISTINCT(HOST) AS HOST FROM SYS.M_SERVICES",
        )
        return [
            Metric(
                "hdb.io.file_handles_count",
                "Number of allocated file handles on the host",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT OPEN_FILE_COUNT FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.io.aysnc_requests_count",
                "Number of active asynchronous input and/or output requests on the host",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACTIVE_ASYNC_IO_COUNT FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
        ]

    def _metrics_column_store_record_locks(self, cluster_id: int) -> List[Metric]:
        indom_table_partitions = self._build_instance_domain(
            # schema name and table name may contain '.'. To keep the split mechanism working, escape a single '.'
            "SELECT HOST, PORT, SCHEMA_NAME, TABLE_NAME, PART_ID FROM SYS.M_CS_TABLES ORDER BY HOST,PORT,SCHEMA_NAME,TABLE_NAME,PART_ID;"
        )
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST,PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )
        return [
            Metric(
                "hdb.record_locks.memory_allocated_bytes",
                "Allocated memory for record locks in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_table_partitions,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT ALLOCATED_MEMORY_SIZE FROM SYS.M_CS_RECORD_LOCK_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SCHEMA_NAME=:SCHEMA_NAME AND TABLE_NAME=:TABLE_NAME AND PART_ID=:PART_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.record_locks.memory_used_bytes",
                "Used memory for record locks in bytes",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_table_partitions,
                    PM_SEM_INSTANT,
                    Metric.UNITS_BYTE,
                ),
                "SELECT USED_MEMORY_SIZE FROM SYS.M_CS_RECORD_LOCK_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SCHEMA_NAME=:SCHEMA_NAME AND TABLE_NAME=:TABLE_NAME AND PART_ID=:PART_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.record_locks.acquired_count",
                "Number of locks that are currently acquired",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_table_partitions,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACQUIRED_LOCK_COUNT FROM SYS.M_CS_RECORD_LOCK_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND SCHEMA_NAME=:SCHEMA_NAME AND TABLE_NAME=:TABLE_NAME AND PART_ID=:PART_ID",
                min_hana_revision=_HANA2_SPS_04,
            ),
            Metric(
                "hdb.record_locks.total_waits_count",
                "Accumulated lock wait count for record locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_COUNTER,
                    Metric.UNITS_COUNT,
                ),
                "SELECT TOTAL_LOCK_WAITS FROM SYS.M_LOCK_WAITS_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND LOCK_TYPE='RECORD'",
            ),
            Metric(
                "hdb.record_locks.total_wait_time_microseconds",
                "Accumulated lock wait time (in microseconds) for record locks for all available services from database start up until the current time.",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MICROSECOND,
                ),
                "SELECT TOTAL_LOCK_WAIT_TIME FROM SYS.M_LOCK_WAITS_STATISTICS WHERE HOST=:HOST AND PORT=:PORT AND LOCK_TYPE='RECORD'",
            ),
        ]

    def _metrics_mvcc(self, cluster_id: int) -> List[Metric]:
        indom_indexservers = self._build_instance_domain(
            "SELECT HOST, PORT FROM SYS.M_SERVICES WHERE SERVICE_NAME='indexserver' ORDER BY HOST,PORT;"
        )
        return [
            Metric(
                "hdb.mvcc.versions_count",
                "Number of all MVCC versions on the host",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT VERSION_COUNT FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.mvcc.data_versions_count",
                "Number of all MVCC data versions per service",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT DATA_VERSION_COUNT FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.mvcc.metadata_versions_count",
                "Number of all MVCC metadata versions per service",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT METADATA_VERSION_COUNT FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.mvcc.acquired_lock_count",
                "Number of acquired records locks",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT ACQUIRED_LOCK_COUNT FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.mvcc.snapshot_lag",
                "Difference between global MVCC timestamp and minimal MVCC timestamp which at least one transaction holds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 4),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT (GLOBAL_MVCC_TIMESTAMP-MIN_MVCC_SNAPSHOT_TIMESTAMP) FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
            Metric(
                "hdb.mvcc.read_write_lag",
                "Difference between minimum closed write transaction ID and what all transactions can see",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 5),
                    PM_TYPE_U64,
                    indom_indexservers,
                    PM_SEM_INSTANT,
                    Metric.UNITS_COUNT,
                ),
                "SELECT (MIN_WRITE_TRANSACTION_ID-MIN_READ_TRANSACTION_ID) FROM SYS.M_MVCC_OVERVIEW WHERE HOST=:HOST AND PORT=:PORT",
            ),
        ]

    def _metrics_aggregated_host_cpu(self, cluster_id: int) -> List[Metric]:
        indom_hosts = self._build_instance_domain(
            "SELECT DISTINCT(HOST) AS HOST FROM SYS.M_SERVICES",
        )
        return [
            Metric(
                "hdb.cpu.total_time_user_milliseconds",
                "CPU time spent in user mode in milliseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 0),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT TOTAL_CPU_USER_TIME FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.cpu.total_time_system_milliseconds",
                "CPU time spent in system mode in milliseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 1),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT TOTAL_CPU_SYSTEM_TIME FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.cpu.total_time_iowait_milliseconds",
                "CPU time spent waiting for I/O in milliseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 2),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT TOTAL_CPU_WIO_TIME FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
            Metric(
                "hdb.cpu.total_time_idle_milliseconds",
                "CPU idle time in milliseconds",
                pmdaMetric(
                    PMDA.pmid(cluster_id, 3),
                    PM_TYPE_U64,
                    indom_hosts,
                    PM_SEM_INSTANT,
                    Metric.UNITS_MILLISECOND,
                ),
                "SELECT TOTAL_CPU_IDLE_TIME FROM SYS.M_HOST_RESOURCE_UTILIZATION WHERE HOST=:HOST",
            ),
        ]


def _int_or_empty_value(val: Any) -> int:
    if val is None:
        return 0
    return int(val)


def _float_or_empty_value(val: Any) -> float:
    if val is None:
        return 0.0
    return float(val)


def _str_or_empty_value(val: Any) -> str:
    if val is None:
        return ""
    return str(val)


# TODO: mapping None to an empty value of correct type might actually be a stupid idea.
#       Should we not rather return an error?
_TYPE_FUNCTIONS = {
    PM_TYPE_U32: _int_or_empty_value,
    PM_TYPE_U64: _int_or_empty_value,
    PM_TYPE_64: _int_or_empty_value,
    PM_TYPE_32: _int_or_empty_value,
    PM_TYPE_FLOAT: _float_or_empty_value,
    PM_TYPE_DOUBLE: _float_or_empty_value,  # python float is C's double
    PM_TYPE_STRING: str,
}


class _Config:
    """
    Configuration structure for the PMDA.
    """

    class HDBConfig:
        """
        SAP HANA database specific section of the configuration.
        """

        SECTION = "hdb"

        def __init__(self, host: str, port: int, user: str, password: str):
            if len(host) == 0:
                raise ValueError("host must not be empty")
            self.host = host

            if port < 1 or port > 65535:
                raise ValueError("port must be in range 1-65535")
            self.port = port

            if len(user) == 0:
                raise ValueError("user must not be empty")
            self.user = user

            if len(password) == 0:
                raise ValueError("password must not be empty")
            self.password = password

    def __init__(
        self,
        hdb_config: HDBConfig,
    ):
        self.hdb_config = hdb_config


def _parse_config(filename: str) -> _Config:
    """
    Reads and parses the configuration file.
    If the configuration file cannot be opened or is invalid, a RuntimeError is raised.
    The object returned by the method is always complete and requires no further validation.
    :param filename: str
        Path to the configuration file.
    :return:
        A valid _Config object.
    """
    # resolve to absolute path such that errors become easier to debug
    conf_file_path = os.path.abspath(filename)
    # configparser returns an empty dir (and no explicit error) if the file does not exist
    if not os.path.exists(conf_file_path):
        raise RuntimeError(f"Config file ({conf_file_path}) does not exist")

    # Python < 3.2 compat
    if sys.version_info[0] >= 3 and sys.version_info[1] >= 2:
        parser = configparser.ConfigParser()
    else:
        parser = configparser.SafeConfigParser()
    parser.read(conf_file_path)

    # hana config parsing
    try:
        hdb_config = _Config.HDBConfig(
            host=parser.get(_Config.HDBConfig.SECTION, "host"),
            port=parser.getint(_Config.HDBConfig.SECTION, "port"),
            user=parser.get(_Config.HDBConfig.SECTION, "user"),
            password=parser.get(_Config.HDBConfig.SECTION, "password"),
        )
    except configparser.Error as parse_err:
        raise RuntimeError(
            f"Config file ({conf_file_path}) is illegal: {parse_err}"
        ) from parse_err
    except ValueError as value_err:
        raise RuntimeError(
            f"Config file ({conf_file_path}) has illegal {_Config.HDBConfig.SECTION} options: {value_err}"
        ) from value_err

    config = _Config(hdb_config)
    return config


def _main():
    arg_parser = argparse.ArgumentParser(
        description="Performance Co-Pilot (PCP) Performance Metric Domain Agent (PMDA) for SAP HANA (hdb)"
    )
    arg_parser.add_argument(
        "--conf",
        dest="conf",
        action="store",
        default="/var/lib/pcp/pmdas/hdb/pmdahdb.conf",
        help="Path to config file",
    )
    args = arg_parser.parse_args()

    # parse config
    try:
        conf = _parse_config(args.conf)
        hdb_conn = HDBConnection(
            conf.hdb_config.host,
            conf.hdb_config.port,
            conf.hdb_config.user,
            conf.hdb_config.password,
        )
    except RuntimeError as ex:
        print(ex, file=sys.stderr)
        sys.exit(1)
    HdbPMDA(hdb_conn).run()


if __name__ == "__main__":
    _main()
