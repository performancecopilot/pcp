#!/usr/bin/env pmpython

import unittest
from unittest.mock import Mock

from pcp_ps import NoneHandlingPrinterDecorator


class TestNoneHandlingPrinterDecorator(unittest.TestCase):
    def test_print_report_with_none(self):
        printer = Mock()
        printer_decorator = NoneHandlingPrinterDecorator(printer)

        printer_decorator.Print(None)

        printer.Print.assert_called_once_with('?')


if __name__ == '__main__':
    unittest.main()
