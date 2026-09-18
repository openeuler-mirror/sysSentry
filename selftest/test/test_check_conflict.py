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

import os
import signal

import pytest

import syssentry.utils as syssentry_utils
from syssentry.global_values import InspectTask
from syssentry.utils import cmdline_matches


def make_task(name, task_start, conflict):
    """create an InspectTask with the given task_start and conflict mode"""
    task = InspectTask(name, "ONESHOT", None, None, task_start, None)
    task.conflict = conflict
    return task


def write_cmdline(tmp_path, pid, argv):
    """write a /proc/<pid>/cmdline-like file (NUL separated) under tmp_path/proc"""
    proc_dir = tmp_path / "proc"
    proc_dir.mkdir(exist_ok=True)
    pid_dir = proc_dir / str(pid)
    pid_dir.mkdir(exist_ok=True)
    content = b"\0".join(argv) + b"\0"
    (pid_dir / "cmdline").write_bytes(content)


def patch_procfs(monkeypatch, tmp_path):
    """point /proc listing and cmdline reading at files under tmp_path/proc"""
    real_listdir = os.listdir
    proc_dir = tmp_path / "proc"
    proc_dir.mkdir(exist_ok=True)
    monkeypatch.setattr(os, "listdir", lambda _p: real_listdir(proc_dir))
    monkeypatch.setattr(syssentry_utils, "cmdline_path",
                        lambda pid: str(proc_dir / str(pid) / "cmdline"))


def record_kills(monkeypatch, tmp_path):
    """patch os.kill to record signals and simulate process exit (remove cmdline
    file after any signal so wait_exit sees the process as gone)
    """
    sent = []
    proc_dir = tmp_path / "proc"

    def fake_kill(pid, sig):
        sent.append((pid, sig))
        cmdline_file = proc_dir / str(pid) / "cmdline"
        cmdline_file.unlink(missing_ok=True)

    monkeypatch.setattr(os, "kill", fake_kill)
    return sent


class TestCmdlineMatches:
    """Test cases for syssentry.utils.cmdline_matches"""

    @staticmethod
    def test_exact_argv_match(tmp_path, monkeypatch):
        write_cmdline(tmp_path, 100, [b"/usr/bin/cpu_sentry"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(100, ["/usr/bin/cpu_sentry"]) is True

    @staticmethod
    def test_prefix_match_with_extra_args(tmp_path, monkeypatch):
        """a process with extra trailing arguments must not match: the whole
        argv (command and every argument) has to be exactly the same
        """
        write_cmdline(tmp_path, 100, [b"/usr/bin/python3", b"/usr/bin/ai_block_io", b"--verbose"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(
            100, ["/usr/bin/python3", "/usr/bin/ai_block_io"]) is False

    @staticmethod
    def test_exact_match_with_args(tmp_path, monkeypatch):
        write_cmdline(tmp_path, 100, [b"/usr/bin/cpu_sentry", b"--verbose"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(
            100, ["/usr/bin/cpu_sentry", "--verbose"]) is True

    @staticmethod
    def test_substring_not_matched(tmp_path, monkeypatch):
        """a process whose cmdline merely contains the target as substring must not match"""
        # editor opened on the plugin path, and a longer binary name
        write_cmdline(tmp_path, 100, [b"/usr/bin/vim", b"/usr/bin/cpu_sentry"])
        write_cmdline(tmp_path, 101, [b"/usr/bin/cpu_sentry_backup"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(100, ["/usr/bin/cpu_sentry"]) is False
        assert cmdline_matches(101, ["/usr/bin/cpu_sentry"]) is False

    @staticmethod
    def test_generic_command_not_matched(tmp_path, monkeypatch):
        """task_start=python3 must not match a python3 interpreter running another script"""
        write_cmdline(tmp_path, 100, [b"/usr/bin/python3", b"/usr/bin/some_service.py"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(100, ["python3", "/usr/bin/ai_block_io"]) is False

    @staticmethod
    def test_exact_argv_match_for_generic_command(tmp_path, monkeypatch):
        """matching is token-exact: an interpreter running the same script is still found"""
        write_cmdline(tmp_path, 100, [b"python3", b"/usr/bin/ai_block_io"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(100, ["python3", "/usr/bin/ai_block_io"]) is True
        # a differently-resolved interpreter path is a different command line
        assert cmdline_matches(100, ["/usr/bin/python3", "/usr/bin/ai_block_io"]) is False

    @staticmethod
    def test_shorter_cmdline_not_matched(tmp_path, monkeypatch):
        write_cmdline(tmp_path, 100, [b"/usr/bin/cpu_sentry"])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(
            100, ["/usr/bin/cpu_sentry", "--verbose", "--debug"]) is False

    @staticmethod
    def test_missing_cmdline_file(tmp_path, monkeypatch):
        """a vanished process (no /proc entry) does not match"""
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(999, ["/usr/bin/cpu_sentry"]) is False

    @staticmethod
    def test_empty_cmdline_file(tmp_path, monkeypatch):
        write_cmdline(tmp_path, 100, [])
        patch_procfs(monkeypatch, tmp_path)
        assert cmdline_matches(100, ["/usr/bin/cpu_sentry"]) is False


class TestCheckConflict:
    """Test cases for InspectTask.check_conflict"""

    @staticmethod
    def test_kill_mode_kills_only_matching_processes(tmp_path, monkeypatch):
        task = make_task("test_kill", "/usr/bin/test_task --flag", "kill")
        # the process with exactly the same argv plus unrelated processes
        # sharing substrings or running with different arguments
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task", b"--flag"])
        write_cmdline(tmp_path, 101, [b"/usr/bin/vim", b"/usr/bin/test_task"])
        write_cmdline(tmp_path, 102, [b"/usr/bin/test_task_helper"])
        write_cmdline(tmp_path, 103, [b"/usr/bin/test_task", b"--other-flag"])
        write_cmdline(tmp_path, 104, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        killed = record_kills(monkeypatch, tmp_path)
        assert task.check_conflict() is True
        assert killed == [(100, signal.SIGTERM)]

    @staticmethod
    def test_kill_mode_generic_command_not_refused_by_check_conflict(tmp_path, monkeypatch):
        """the absolute-path guard moved to config load (load_mods); check_conflict
        no longer refuses a generic command, it proceeds and kills the exact-argv
        match only
        """
        task = make_task("test_kill_generic", "python3 /usr/bin/ai_block_io", "kill")
        write_cmdline(tmp_path, 100, [b"python3", b"/usr/bin/ai_block_io"])
        patch_procfs(monkeypatch, tmp_path)
        killed = record_kills(monkeypatch, tmp_path)
        assert task.check_conflict() is True
        assert killed == [(100, signal.SIGTERM)]

    @staticmethod
    def test_kill_mode_allows_absolute_path_command(tmp_path, monkeypatch):
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        patch_procfs(monkeypatch, tmp_path)
        assert task.check_conflict() is True

    @staticmethod
    def test_kill_mode_no_conflict_process(tmp_path, monkeypatch):
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/other_task"])
        patch_procfs(monkeypatch, tmp_path)
        killed = record_kills(monkeypatch, tmp_path)
        assert task.check_conflict() is True
        assert not killed 

    @staticmethod
    def test_down_mode_rejects_start_on_conflict(tmp_path, monkeypatch):
        task = make_task("test_down", "/usr/bin/test_task", "down")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        assert task.check_conflict() is False

    @staticmethod
    def test_down_mode_allows_start_without_conflict(monkeypatch):
        task = make_task("test_down", "/usr/bin/test_task", "down")
        monkeypatch.setattr(os, "listdir", lambda _p: [])
        assert task.check_conflict() is True

    @staticmethod
    def test_self_pid_never_matches(tmp_path, monkeypatch):
        """the syssentry process itself must not be treated as a conflict"""
        task = make_task("test_self", "/usr/bin/test_task", "kill")
        self_pid = os.getpid()
        # craft a cmdline that equals task_start, attributed to our own pid
        write_cmdline(tmp_path, self_pid, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        killed = record_kills(monkeypatch, tmp_path)
        monkeypatch.setattr(os, "getpid", lambda: self_pid)
        assert task.check_conflict() is True
        assert not killed 

    @staticmethod
    def test_kill_mode_recheck_skips_recycled_pid(tmp_path, monkeypatch):
        """a pid whose cmdline vanishes between scan and kill must not be killed"""
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        real_listdir = os.listdir
        proc_dir = tmp_path / "proc"

        calls = {"n": 0}

        def flakycmdline_path(pid):
            # first probe (scan) sees the cmdline, later probes (recheck) do not
            calls["n"] += 1
            if calls["n"] == 1:
                return str(proc_dir / str(pid) / "cmdline")
            return str(proc_dir / "missing" / "cmdline")

        monkeypatch.setattr(os, "listdir", lambda _p: real_listdir(proc_dir))
        monkeypatch.setattr(syssentry_utils, "cmdline_path", flakycmdline_path)
        killed = record_kills(monkeypatch, tmp_path)
        assert task.check_conflict() is True
        assert not killed 

    @staticmethod
    def test_invalid_task_start_returns_false():
        task = make_task("test_bad", "unmatched 'quote", "kill")
        assert task.check_conflict() is False

    @staticmethod
    def test_empty_task_start_returns_false():
        task = make_task("test_empty", "", "kill")
        assert task.check_conflict() is False

    @staticmethod
    def test_proc_listdir_failure_returns_false(monkeypatch):
        task = make_task("test_kill", "/usr/bin/test_task", "kill")

        def boom(_p):
            raise OSError("procfs unavailable")

        monkeypatch.setattr(os, "listdir", boom)
        assert task.check_conflict() is False

    @staticmethod
    def test_kill_permission_denied_returns_false(tmp_path, monkeypatch):
        """EPERM from os.kill must be detected; check_conflict refuses to start"""
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)

        def deny_kill(pid, sig):
            raise PermissionError("not allowed")

        monkeypatch.setattr(os, "kill", deny_kill)
        assert task.check_conflict() is False

    @staticmethod
    def test_kill_process_already_gone(tmp_path, monkeypatch):
        """ProcessLookupError means the process already exited; treat as success"""
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        proc_dir = tmp_path / "proc"

        def kill_gone(pid, sig):
            (proc_dir / str(pid) / "cmdline").unlink(missing_ok=True)
            raise ProcessLookupError("no such process")

        monkeypatch.setattr(os, "kill", kill_gone)
        assert task.check_conflict() is True

    @staticmethod
    def test_sigterm_ignored_then_sigkill_escalation(tmp_path, monkeypatch):
        """if SIGTERM does not make the process exit, escalate to SIGKILL"""
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        monkeypatch.setattr("syssentry.utils.KILL_VERIFY_TIMEOUT", 0.1)
        monkeypatch.setattr("syssentry.utils.KILL_POLL_INTERVAL", 0.01)

        proc_dir = tmp_path / "proc"
        sent = []

        def stubborn_kill(pid, sig):
            sent.append((pid, sig))
            if sig == signal.SIGKILL:
                (proc_dir / str(pid) / "cmdline").unlink(missing_ok=True)

        monkeypatch.setattr(os, "kill", stubborn_kill)
        assert task.check_conflict() is True
        assert (100, signal.SIGTERM) in sent
        assert (100, signal.SIGKILL) in sent

    @staticmethod
    def test_kill_failure_refuses_start(tmp_path, monkeypatch):
        """when a conflict process cannot be killed, check_conflict returns False"""
        task = make_task("test_kill", "/usr/bin/test_task", "kill")
        write_cmdline(tmp_path, 100, [b"/usr/bin/test_task"])
        patch_procfs(monkeypatch, tmp_path)
        monkeypatch.setattr("syssentry.utils.KILL_VERIFY_TIMEOUT", 0.1)
        monkeypatch.setattr("syssentry.utils.KILL_POLL_INTERVAL", 0.01)

        def immune_kill(pid, sig):
            raise PermissionError("immune")

        monkeypatch.setattr(os, "kill", immune_kill)
        assert task.check_conflict() is False
