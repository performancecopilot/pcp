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

import sys
import unittest
if sys.version_info[0] < 3:
    from mock import Mock
else:
    from unittest.mock import Mock
from pcp_mpstat import NoneHandlingPrinterDecorator

class TestNoneHandlingPrinterDecorator(unittest.TestCase):

    def test_print_report_without_none_values(self):
        printer = Mock()
        printer.Print = Mock()
        printer_decorator = NoneHandlingPrinterDecorator(printer.Print)

        printer_decorator.Print("2016-07-20 IST\tALL\t 1.23\t  2.34\t 3.45\t    4.56\t 5.67\t  6.78\t   7.89\t    8.9\t  1.34\t  2.45")

        printer.Print.assert_called_with("2016-07-20 IST\tALL\t 1.23\t  2.34\t 3.45\t    4.56\t 5.67\t  6.78\t   7.89\t    8.9\t  1.34\t  2.45")

    def test_print_report_with_none_values(self):
        printer = Mock()
        printer_decorator = NoneHandlingPrinterDecorator(printer.Print)

        printer_decorator.Print("2016-07-20 IST\tALL\t 1.23\t  None\t 3.45\t    4.56\t None\t  6.78\t   7.89\t    None\t  1.34\t  2.45")

        printer.Print.assert_called_with("2016-07-20 IST\tALL\t 1.23\t  ?\t 3.45\t    4.56\t ?\t  6.78\t   7.89\t    ?\t  1.34\t  2.45")

if __name__ == "__main__":
    unittest.main()
