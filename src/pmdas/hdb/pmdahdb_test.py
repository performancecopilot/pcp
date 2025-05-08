import os
import tempfile
import unittest
from configparser import MissingSectionHeaderError

from cpmapi import PM_ERR_AGAIN, PM_ERR_INST, PM_ERR_PMID

from pmdahdb import (
    _HANA2_SPS_01,
    _HANA2_SPS_04,
    _HANA2_SPS_05,
    HDBConnection,
    HdbPMDA,
    _hana_revision_included,
    _parse_config,
)


class HdbPMDATest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hdb_host = os.getenv("HDB_HOST")
        if not hdb_host:
            cls.skipTest(
                cls,
                "hdb host for test not specified. Set environment variable 'HDB_HOST'",
            )
        hdb_port = os.getenv("HDB_PORT")
        if not hdb_port:
            cls.skipTest(
                cls,
                "hdb port for test not specified. Set environment variable 'HDB_PORT'",
            )
        hdb_user = os.getenv("HDB_USER")
        if not hdb_user:
            cls.skipTest(
                cls,
                "hdb user for test not specified. Set environment variable 'HDB_USER'",
            )
        hdb_password = os.getenv("HDB_PASSWORD")
        if not hdb_password:
            cls.skipTest(
                cls,
                "hdb password for test not specified. Set environment variable 'HDB_PASSWORD'",
            )
        hdb = HDBConnection(hdb_host, int(hdb_port), hdb_user, hdb_password)
        cls.pmda = HdbPMDA(hdb)

    def test_fetch_callback_unknown_cluster_PM_ERR_PMID(self):
        [value, success] = self.pmda.fetch_callback(999, 0, 0)
        self.assertEqual(value, PM_ERR_PMID)
        self.assertEqual(success, 0)

    def test_fetch_callback_unknown_item_PM_ERR_PMID(self):
        [value, success] = self.pmda.fetch_callback(0, 999, 0)
        self.assertEqual(value, PM_ERR_PMID)
        self.assertEqual(success, 0)

    def test_fetch_callback_unknown_instance_PM_ERR_INST(self):
        [value, success] = self.pmda.fetch_callback(2, 1, 999)
        self.assertEqual(value, PM_ERR_INST)
        self.assertEqual(success, 0)

    def test_fetch_callback_instance_for_NULL_INDOM_metric_ok(self):
        [value, success] = self.pmda.fetch_callback(0, 0, 999)
        self.assertNotEqual(value, PM_ERR_PMID)
        self.assertEqual(success, 1)

    def test_fetch_callback_value_without_instance_domain_ok(self):
        [value, success] = self.pmda.fetch_callback(0, 0, 0)
        self.assertNotEqual(value, PM_ERR_PMID)
        self.assertEqual(success, 1)

    def test_fetch_callback_value_with_instance_ok(self):
        [value, success] = self.pmda.fetch_callback(2, 1, 0)
        self.assertNotEqual(value, PM_ERR_PMID)
        self.assertEqual(success, 1)

    def test_fetch_callback(self):
        # TODO: derive the triplets automatically
        metrics = [(0, i, 0) for i in range(3)]
        metrics += [(1, i, 0) for i in range(4)]
        metrics += [(2, i, 0) for i in range(3)]
        metrics += [(3, i, 0) for i in range(6)]
        metrics += [(4, i, 0) for i in range(18)]
        metrics += [(5, i, 0) for i in range(14)]
        metrics += [(6, i, 0) for i in range(5)]
        metrics += [(7, i, 0) for i in range(7)]
        metrics += [(8, i, 0) for i in range(5)]
        metrics += [(9, i, 0) for i in range(7)]
        metrics += [(10, i, 0) for i in range(22)]
        metrics += [(11, i, 0) for i in range(7)]
        metrics += [(12, i, 0) for i in range(7)]
        metrics += [(13, i, 0) for i in range(2)]
        metrics += [(14, i, 0) for i in range(2)]
        metrics += [(15, i, 0) for i in range(21)]
        metrics += [(16, i, 0) for i in range(5)]
        metrics += [(17, i, 0) for i in range(20)]
        metrics += [(18, i, 0) for i in range(4)]
        metrics += [(19, i, 0) for i in range(6)]
        metrics += [(20, i, 0) for i in range(13)]
        metrics += [(21, i, 0) for i in range(1)]
        metrics += [(22, i, 0) for i in range(4)]
        metrics += [(23, i, 0) for i in range(2)]
        # sanity check that all registered metrics are probed in this test
        self.assertEqual(
            len(self.pmda._metric_lookup),
            len(metrics),
            "not all registered metrics tested",
        )
        for (cluster, item, inst) in metrics:
            [value, success] = self.pmda.fetch_callback(cluster, item, inst)
            self.assertEqual(success, 1, f"cluster={cluster}, item={item}, inst={inst}")
            self.assertNotIn(
                value,
                [PM_ERR_AGAIN, PM_ERR_INST, PM_ERR_PMID],
                f"cluster={cluster}, item={item}, inst={inst}",
            )


class ConfigTest(unittest.TestCase):
    def test_parse_config_file_not_found_error(self):
        with self.assertRaisesRegex(RuntimeError, "does not exist"):
            _parse_config("does-not-exist")

    def test_parse_config_valid_config_ok(self):
        host = "localhost"
        port = 39015
        user = "user"
        password = "password"
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                bytes(
                    f"""
                [hdb]
                host={host}
                port={port}
                user={user}
                password={password}
                """,
                    encoding="utf-8",
                )
            )
            f.seek(0)
            config = _parse_config(f.name)
        self.assertEqual(config.hdb_config.host, host)
        self.assertEqual(config.hdb_config.port, port)
        self.assertEqual(config.hdb_config.user, user)
        self.assertEqual(config.hdb_config.password, password)

    def test_parse_config_missing_hdb_section_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                host=localhost
                port=39015
                user=user
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(MissingSectionHeaderError, "section headers"):
                _parse_config(f.name)

    def test_parse_config_missing_hdb_host_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                port=39015
                user=user
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "host"):
                _parse_config(f.name)

    def test_parse_config_empty_hdb_host_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=
                port=39015
                user=user
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "host"):
                _parse_config(f.name)

    def test_parse_config_missing_hdb_port_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                user=user
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "port"):
                _parse_config(f.name)

    def test_parse_config_invalid_hdb_port_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                port=0
                user=user
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "port"):
                _parse_config(f.name)

    def test_parse_config_missing_hdb_user_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                port=39015
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "user"):
                _parse_config(f.name)

    def test_parse_config_empty_hdb_user_option_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                port=39015
                user=
                password=password
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "user"):
                _parse_config(f.name)

    def test_parse_config_missing_hdb_password_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                port=39015
                user=user
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "password"):
                _parse_config(f.name)

    def test_parse_config_empty_hdb_password_error(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                b"""
                [hdb]
                host=localhost
                port=39015
                user=user
                password=
                """
            )
            f.seek(0)
            with self.assertRaisesRegex(RuntimeError, "password"):
                _parse_config(f.name)

    def test_parse_config_quoted_hdb_password_ok(self):
        password = '"?Ö"'
        with tempfile.NamedTemporaryFile() as f:
            f.write(
                bytes(
                    f"""
                       [hdb]
                       host=localhost
                       port=39015
                       user=user
                       password={password}
                       """,
                    encoding="utf-8",
                )
            )
            f.seek(0)
            config = _parse_config(f.name)
        self.assertEqual(config.hdb_config.password, password)


class RevisionCompatabilityTest(unittest.TestCase):
    def test_revision_less_than_min_incompatible(self):
        self.assertFalse(_hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_04, None))
        self.assertFalse(
            _hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_04, _HANA2_SPS_05)
        )

    def test_revision_with_unspecified_min_max_compatible(self):
        self.assertTrue(_hana_revision_included(_HANA2_SPS_05, None, None))

    def test_revision_equal_to_min_compatible(self):
        self.assertTrue(_hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_01, None))
        self.assertTrue(
            _hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_01, _HANA2_SPS_05)
        )

    def test_revision_in_range_compatible(self):
        self.assertTrue(
            _hana_revision_included(_HANA2_SPS_04, _HANA2_SPS_01, _HANA2_SPS_05)
        )
        self.assertTrue(_hana_revision_included(_HANA2_SPS_04, None, _HANA2_SPS_05))
        self.assertTrue(_hana_revision_included(_HANA2_SPS_04, _HANA2_SPS_01, None))

    def test_revision_equal_to_min_max_compatible(self):
        self.assertTrue(
            _hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_01, _HANA2_SPS_01)
        )

    def test_revision_equal_to_max_compatible(self):
        self.assertTrue(_hana_revision_included(_HANA2_SPS_04, None, _HANA2_SPS_04))
        self.assertTrue(
            _hana_revision_included(_HANA2_SPS_04, _HANA2_SPS_01, _HANA2_SPS_04)
        )

    def test_revision_greater_than_max_incompatible(self):
        self.assertFalse(_hana_revision_included(_HANA2_SPS_05, None, _HANA2_SPS_04))
        self.assertFalse(
            _hana_revision_included(_HANA2_SPS_05, _HANA2_SPS_01, _HANA2_SPS_04)
        )

    def test_none_revision_error(self):
        with self.assertRaises(ValueError):
            self.assertFalse(_hana_revision_included(None, None, None))
        with self.assertRaises(ValueError):
            self.assertFalse(_hana_revision_included(None, _HANA2_SPS_01, None))
        with self.assertRaises(ValueError):
            self.assertFalse(
                _hana_revision_included(None, _HANA2_SPS_01, _HANA2_SPS_04)
            )
        with self.assertRaises(ValueError):
            self.assertFalse(_hana_revision_included(None, None, _HANA2_SPS_04))

    def test_min_greater_max_error(self):
        with self.assertRaises(ValueError):
            _hana_revision_included(_HANA2_SPS_01, _HANA2_SPS_04, _HANA2_SPS_01)


if __name__ == "__main__":
    unittest.main()
