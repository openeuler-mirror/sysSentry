# coding: utf-8
# Copyright (c) 2024 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

import unittest
import tempfile
import os

from syssentry.load_mods import (
    validate_alarm_id,
    validate_alarm_clear_time,
    validate_env_file,
    validate_conflict,
)
from syssentry.global_values import DEFAULT_ALARM_CLEAR_TIME, DEFAULT_CONFLICT


class TestValidateAlarmId(unittest.TestCase):
    """Test cases for alarm_id validation"""

    def test_valid_alarm_id(self):
        """Test with valid alarm_id values within allowed range"""
        self.assertEqual(validate_alarm_id("test_mod", "1001"), 1001)
        self.assertEqual(validate_alarm_id("test_mod", "1128"), 1128)
        self.assertEqual(validate_alarm_id("test_mod", "1001 "), 1001)
        self.assertEqual(validate_alarm_id("test_mod", " 1001 "), 1001)
        self.assertEqual(validate_alarm_id("test_mod", "   1001"), 1001)
        self.assertEqual(validate_alarm_id("test_mod", "  1128   "), 1128)
        self.assertEqual(validate_alarm_id("test_mod", " 1128 "), 1128)

    def test_invalid_alarm_id(self):
        """Test with invalid alarm_id values outside allowed range or non-integer"""
        self.assertIsNone(validate_alarm_id("test_mod", "0"))
        self.assertIsNone(validate_alarm_id("test_mod", "1"))
        self.assertIsNone(validate_alarm_id("test_mod", "100"))
        self.assertIsNone(validate_alarm_id("test_mod", "12345"))
        self.assertIsNone(validate_alarm_id("test_mod", "abc"))
        self.assertIsNone(validate_alarm_id("test_mod", "12.5"))
        self.assertIsNone(validate_alarm_id("test_mod", "1.0"))
        self.assertIsNone(validate_alarm_id("test_mod", ""))
        self.assertIsNone(validate_alarm_id("test_mod", None))
        self.assertIsNone(validate_alarm_id("test_mod", "  "))
        self.assertIsNone(validate_alarm_id("test_mod", "1abc"))
        self.assertIsNone(validate_alarm_id("test_mod", "abc123"))
        self.assertIsNone(validate_alarm_id("test_mod", "-1"))
        self.assertIsNone(validate_alarm_id("test_mod", "-100"))
        # Python int() accepts spaces around number, this is valid behavior
        self.assertIsNone(validate_alarm_id("test_mod", "  100  "))
        self.assertIsNone(validate_alarm_id("test_mod", "0x10"))  # hex format


class TestValidateAlarmClearTime(unittest.TestCase):
    """Test cases for alarm_clear_time validation"""

    def test_valid_integer_positive(self):
        """Test with valid positive integer values"""
        self.assertEqual(validate_alarm_clear_time("test_mod", "1"), 1)
        self.assertEqual(validate_alarm_clear_time("test_mod", "15"), 15)
        self.assertEqual(validate_alarm_clear_time("test_mod", "30"), 30)
        self.assertEqual(validate_alarm_clear_time("test_mod", "100"), 100)
        self.assertEqual(validate_alarm_clear_time("test_mod", "3600"), 3600)

    def test_invalid_non_integer(self):
        """Test with non-integer values, should return default"""
        self.assertEqual(validate_alarm_clear_time("test_mod", "abc"), DEFAULT_ALARM_CLEAR_TIME)
        self.assertEqual(validate_alarm_clear_time("test_mod", "12.5"), DEFAULT_ALARM_CLEAR_TIME)
        self.assertEqual(validate_alarm_clear_time("test_mod", ""), DEFAULT_ALARM_CLEAR_TIME)
        self.assertEqual(validate_alarm_clear_time("test_mod", None), DEFAULT_ALARM_CLEAR_TIME)
        self.assertEqual(validate_alarm_clear_time("test_mod", "  "), DEFAULT_ALARM_CLEAR_TIME)

    def test_valid_zero(self):
        """Test with zero value, should be accepted"""
        self.assertEqual(validate_alarm_clear_time("test_mod", "0"), 0)

    def test_invalid_negative(self):
        """Test with negative values, should return default"""
        self.assertEqual(validate_alarm_clear_time("test_mod", "-1"), DEFAULT_ALARM_CLEAR_TIME)
        self.assertEqual(validate_alarm_clear_time("test_mod", "-100"), DEFAULT_ALARM_CLEAR_TIME)

    def test_edge_cases_spaces_accepted(self):
        """Test that spaces around the number are accepted"""
        # Python int() accepts spaces around number, this is valid behavior
        self.assertEqual(validate_alarm_clear_time("test_mod", "  30  "), 30)

    def test_invalid_hex_format(self):
        """Test with hex format string, should return default"""
        self.assertEqual(validate_alarm_clear_time("test_mod", "0x10"), DEFAULT_ALARM_CLEAR_TIME)


class TestValidateEnvFile(unittest.TestCase):
    """Test cases for env_file validation"""

    def test_valid_env_file(self):
        """Test with a valid existing file"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as tmp_file:
            tmp_file.write("TEST_VAR=test_value\n")
            tmp_path = tmp_file.name

        try:
            result = validate_env_file("test_mod", tmp_path)
            self.assertEqual(result, os.path.normpath(tmp_path))
        finally:
            os.unlink(tmp_path)

    def test_nonexistent_file(self):
        """Test with a non-existent file path"""
        result = validate_env_file("test_mod", "/nonexistent/path/to/file.env")
        self.assertEqual(result, "")

    def test_empty_path(self):
        """Test with empty path"""
        self.assertEqual(validate_env_file("test_mod", ""), "")
        self.assertEqual(validate_env_file("test_mod", None), "")

    def test_directory_instead_of_file(self):
        """Test when path points to a directory instead of file"""
        result = validate_env_file("test_mod", "/tmp")
        self.assertEqual(result, "")


class TestValidateConflict(unittest.TestCase):
    """Test cases for conflict validation"""

    def test_valid_values_lowercase(self):
        """Test with valid conflict values in lowercase"""
        self.assertEqual(validate_conflict("test_mod", "up"), "up")
        self.assertEqual(validate_conflict("test_mod", "down"), "down")
        self.assertEqual(validate_conflict("test_mod", "kill"), "kill")

    def test_values_with_whitespace(self):
        """Test that whitespace around values is trimmed"""
        self.assertEqual(validate_conflict("test_mod", " up "), "up")
        self.assertEqual(validate_conflict("test_mod", "  down  "), "down")
        self.assertEqual(validate_conflict("test_mod", "\tkill\t"), "kill")

    def test_invalid_values_uppercase(self):
        """Test with uppercase values, should return default"""
        self.assertEqual(validate_conflict("test_mod", "UP"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "DOWN"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "KILL"), DEFAULT_CONFLICT)

    def test_invalid_values_mixed_case(self):
        """Test with mixed case values, should return default"""
        self.assertEqual(validate_conflict("test_mod", "Up"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "Down"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "Kill"), DEFAULT_CONFLICT)

    def test_invalid_values(self):
        """Test with invalid conflict values, should return default"""
        self.assertEqual(validate_conflict("test_mod", "invalid"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "pause"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "stop"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", ""), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", None), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "  "), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "updown"), DEFAULT_CONFLICT)
        self.assertEqual(validate_conflict("test_mod", "up kill"), DEFAULT_CONFLICT)


class TestDefaultValues(unittest.TestCase):
    """Test that default values are correctly used"""

    def test_default_alarm_clear_time(self):
        """Test that default alarm_clear_time value is 15"""
        self.assertEqual(DEFAULT_ALARM_CLEAR_TIME, 15)

    def test_default_conflict(self):
        """Test that default conflict value is 'up'"""
        self.assertEqual(DEFAULT_CONFLICT, "up")


if __name__ == '__main__':
    unittest.main()