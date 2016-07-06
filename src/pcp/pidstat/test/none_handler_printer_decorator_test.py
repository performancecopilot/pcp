#!/usr/bin/env pmpython
#
# Copyright (C) 2016 Sitaram Shelke.
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
#

import unittest
from mock import Mock
from pcp_pidstat import NoneHandlingPrinterDecorator
class TestNoneHandlingPrinterDecorator(unittest.TestCase):

    def test_print_report_without_none_values(self):
        printer = Mock()
        printer.Print = Mock()
        printer_decorator = NoneHandlingPrinterDecorator(printer)

        printer_decorator.Print("123\t1000\t1\t2.43\t1.24\t0.0\t3.67\t1\tprocess_1")

        printer.Print.assert_called_with("123\t1000\t1\t2.43\t1.24\t0.0\t3.67\t1\tprocess_1")

    def test_print_report_with_none_values(self):
        printer = Mock()
        printer.Print = Mock()
        printer_decorator = NoneHandlingPrinterDecorator(printer)

        printer_decorator.Print("123\t1000\t1\tNone\t1.24\t0.0\tNone\t1\tprocess_1")

        printer.Print.assert_called_with("123\t1000\t1\t?\t1.24\t0.0\t?\t1\tprocess_1")

if __name__ == "__main__":
    unittest.main()
