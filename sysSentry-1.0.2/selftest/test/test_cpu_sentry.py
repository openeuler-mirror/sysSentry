import pytest

from unittest.mock import mock_open, patch
from syssentry.sentry_config import CpuPluginsParamsConfig
from syssentry.cpu_sentry import CpuSentry

class TestCaseCpuSentry:

    @pytest.mark.parametrize(
            "cpu_input, except_cpu_list",
            [
                ("2-4", [2,3,4]),
                ("4-2", []),
                ("2-x", []),
                ("-1,2-4", []),
                ("x,2-4", []),
                (",,2-4", []),
                ("0,2-4", [0, 2,3,4]),
                ("0, 2-4", []),
                ("1-", []),
                ("-1", []),
                ("", []),
                ("1-3,5-8", [1,2,3,5,6,7,8]),
                ("1-3,8", [1,2,3,8]),
                ("11", [11]),
                ("1-3,5-8,12,16-20", [1,2,3,5,6,7,8,12,16,17,18,19,20]),
                ("1-3-6", []),
                ("xxx", []),
            ],
    )
    def test_cpu_format_convert_to_list(self, cpu_input, except_cpu_list):
        assert CpuSentry.cpu_format_convert_to_list(cpu_input) == except_cpu_list

    @pytest.mark.parametrize(
            "config_cpu_list, except_valid",
            [
                ("0", True),
                ("1-2", True),
                ("1- 2", False),
                ("1,3", True),
                ("1,3,", False),
                ("1-3,6,", False),
                ("1-3,6", True),
                ("1-3,5-8", True),
                ("1-3,8", True),
                ("1-3,5,8", True),
                ("11", True),
                ("1-10,21,24,35-38", True),
                ("1-10,21,24,-38", False),
                ("1-10,21,24;38", False),
                ("-0", False),
                ("0,2-4", True),
                ("0, 2-4", False),
                ("1-3-6", False),
                ("", False),
                ("xxx", False),
                ("1-3,,", False),
                ("1-3,,6", False),
                ("1-3;6", False),
                ("1-3;", False),
                (";1-3", False),
                ("xx?ds", False),
                ("-", False),
                ("-10", False),
                ("10-", False),
                ("1--10", False),
                ("0-2a", False),
                ("0,-1", False),
                (",,,", False),
                (",", False),
            ],
    )
    def test_is_valid_cpu_input(self, config_cpu_list, except_valid):
        assert CpuPluginsParamsConfig.is_valid_cpu_input(config_cpu_list) == except_valid

    # test read file "/sys/devices/system/cpu/offline" to get offline cpu info
    def test_get_cpu_list_from_sys_file(self):
        test_cases = [
                ('0,2,4', [0, 2, 4]),
                ('0-4', [0, 1, 2, 3, 4]),
                ('0-4,9,12-14,18,20,23-24', [0, 1, 2, 3, 4, 9, 12, 13, 14, 18, 20, 23, 24]),
                ('\n', []),
                ('0-4', [0,1,2,3,4]),
                # invalid data, all result is empty list
                ('0-4, x', []),
                ('0-4,', []),
                ('0-4 ', []),
                ("x", []),
        ]
        for input_data, expected_output in test_cases:
            with patch('builtins.open', new_callable=mock_open, read_data=input_data):
                assert CpuSentry.get_cpu_list_from_sys_file("/sys/devices/system/cpu/offline") == expected_output
