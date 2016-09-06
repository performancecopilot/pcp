#!/usr/bin/env pmpython
#
# Copyright (C) 2016 Ryan Doyle
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

import unittest
import cpmapi
from pcp import pmapi

class TestPmErr(unittest.TestCase):

    def test_errno_returns_the_error_number_that_caused_the_exception(self):
        error = pmapi.pmErr(cpmapi.PM_ERR_PMID)

        self.assertEqual(error.errno, cpmapi.PM_ERR_PMID)

    def test_message_returns_the_string_representation_of_the_error(self):
        error = pmapi.pmErr(cpmapi.PM_ERR_PMID)

        self.assertEqual(error.message(), "Unknown or illegal metric identifier")

    def test_message_with_additional_arguments_appends_the_arguments_to_the_message(self):
        error = pmapi.pmErr(cpmapi.PM_ERR_PMID, "arg1", "arg2")

        self.assertEqual(error.message(), "Unknown or illegal metric identifier arg1 arg2")