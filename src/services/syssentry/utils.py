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

"""
some common function
"""
import logging
import os
import signal
import socket
import subprocess
import shlex
import re
import time

from datetime import datetime, timezone, timedelta

# Security: Maximum allowed message length to prevent DoS attacks
MAX_MSG_LEN = 10 * 1024 * 1024  # 10MB

# Timeouts used when killing a conflicting process and verifying it has exited.
KILL_VERIFY_TIMEOUT = 2.0
KILL_POLL_INTERVAL = 0.05

ENV_BLACKLIST_PATTERNS = [
    r"^LD_",                 # LD_PRELOAD, LD_LIBRARY_PATH 等（可劫持动态链接）
    r"^MALLOC_",             # 一系列控制堆内存分配行为的变量，可能被用于触发非预期状态或拒绝服务
    r"^BASH_ENV$",           # Bash 自动执行脚本
    r"^ENV$",                # POSIX sh 自动执行脚本
    r"^IFS$",                # Shell 内部字段分隔符（可能破坏命令解析）
    r"^PATH$",               # 危险！可让子进程执行恶意程序
    r"^PYTHONPATH$",         # 可让Python导入恶意模块
    r"^NODE_PATH$",          # Node.js 模块路径劫持
    r"^PERL5LIB$",           # Perl 库路径劫持
    r"^PERL5OPT$",           # Perl解释器的启动选项环境变量
    r"^TMPDIR$",             # 某些场景下可能导致文件覆盖
    r"^HOME$",               # 可能改变配置文件读取路径
    r"^HISTFILE$",           # 可能覆盖历史记录
    r"^JAVA_TOOL_OPTIONS$",  # 此变量可以传入JVM启动参数，在程序运行前执行任意代码
    r"^LUA_INIT$",           # Lua解释器会在运行主脚本前执行此变量中的代码或指定的文件
    r"^RUBYOPT$",            # Ruby解释器预设命令行参数，如果被攻击者控制，就可能被用来注入恶意代码
    r"^R_PROFILE_USER$",     # 该变量指向的文件‌不校验所有权或签名‌，若被恶意篡改，任意R代码会静默执行，可能导致命令注入
]

CMD_STRING_WHITELIST = ["kill $pid"]

# Base safe characters: letters, numbers, spaces, comma, hyphens, underscore, dot, slash
CMD_SAFE_PATTERN = re.compile(r'^[a-zA-Z0-9 ,_./-]+$')


def recv_all(sock: socket.socket, length: int) -> bytes:
    """Receive exactly `length` bytes from the socket.

    socket.recv() may return fewer bytes than requested, especially on
    stream sockets. This function loops until all requested bytes are
    received or the connection is closed / an error occurs.

    Returns the received data as bytes.
    Raises ConnectionError if the peer closes the connection before all
    bytes are received.
    Raises OSError (propagated from sock.recv()) on other socket errors,
    e.g. timeout or connection reset.
    """
    if length <= 0:
        return b""
    data = b""
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            # Connection closed by peer before all data received
            raise ConnectionError(
                f"Connection closed: received {len(data)} of {length} bytes"
            )
        data += chunk
    return data


def validate_command_string(cmd: str) -> bool:
    """
    Validate if a command string contains only safe characters

    Args:
        cmd: Command string to validate

    Returns:
        bool: True if valid, False otherwise
    """
    # 1. Type check
    if not isinstance(cmd, str):
        return False

    # 2. Empty string check
    if not cmd or not cmd.strip():
        return False

    # 3. whitelist check
    if cmd in CMD_STRING_WHITELIST:
        return True

    # 4. Consecutive dots are not allowed, e.g., '..'
    if ".." in cmd:
        return False

    # 5. Additional security checks: check for dangerous patterns
    # Check for dangerous command combinations
    dangerous_patterns = [
        r'[;&|`]',           # Shell operators
        r'\$\(',             # Command substitution
        r'\(\(',             # Arithmetic operations
        r'\$\{',             # Variable substitution
        r'\n',               # Newline
        r'\r',               # Carriage return
        r'\t',               # Tab
        r'\s+\|\s+',         # Pipe operator
        r'\s+>\s+',          # Output redirection
        r'\s+<\s+',          # Input redirection
        r'&&',               # Logical AND
        r'\|\|',             # Logical OR
        r'\\',               # Backslash escape
        r'\$\w+',            # Variable reference
    ]
    for dangerous in dangerous_patterns:
        if re.search(dangerous, cmd):
            return False

    # 6. Build allowed character set
    if not CMD_SAFE_PATTERN.match(cmd):
        return False
    return True


def run_cmd(cmd):
    """run cmd use subprocess.run"""
    if not validate_command_string(cmd):
        return None
    result = subprocess.run(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    return result


def run_popen(cmd):
    """run cmd use subprocess.Popen"""
    if not validate_command_string(cmd):
        return None
    pipe = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return pipe


def is_exists_cmd(cmd: str) -> bool:
    """Checking Whether a Command Exists in the Environment"""
    res = run_cmd(f"/usr/bin/which {cmd}")
    if not res:
        return False
    if res.returncode:
        return False
    return True


def get_process_pid(process_name):
    """get the PID of a specified program."""
    process_pid = -1
    if "/" in process_name:
        process_name = process_name.split("/")[-1]
    res = run_cmd('/usr/bin/pgrep -x {}'.format(process_name))
    if not res:
        return process_pid
    if res.returncode == 0:
        process_pid = res.stdout.decode().strip()
        try:
            process_pid = int(process_pid)
        except ValueError:
            process_pid = 0
    return process_pid


def get_current_time_string():
    """get time"""
    current_utc_time = datetime.now(timezone.utc)
    utc8_timezone = timezone(timedelta(hours=8))
    return current_utc_time.astimezone(utc8_timezone).strftime("%Y-%m-%d %H:%M:%S")


def execute_command(cmd_list, timeout=None):
    try:
        process = subprocess.run(
            cmd_list,
            shell=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding="utf-8",
            timeout=timeout,
        )
        returncode = process.returncode
        if returncode != 0:
            logging.error("execute command with illegal returncode")
            return None
        return process.stdout
    except subprocess.TimeoutExpired:
        logging.error("execute command timeout: %s", cmd_list[0] if cmd_list else "")
        return None
    except Exception as e:
        logging.error("failed to execute command with error: %s", str(e))
        return None


def is_dangerous_env_key(env_name):
    """Check whether the env is in the blacklist."""
    for pattern in ENV_BLACKLIST_PATTERNS:
        if re.match(pattern, env_name, re.IGNORECASE):
            return True
    return False


def cmdline_path(pid):
    """get the path of the /proc cmdline file of process pid"""
    return "/proc/%d/cmdline" % pid


def cmdline_matches(pid, target_argv):
    """check whether the command line of process pid equals target_argv exactly.

    A process is a conflict only when its argv (command and every argument) is
    identical to target_argv: no more and no fewer tokens. An empty target_argv
    never matches, so processes with no arguments are never reported.
    """
    if not target_argv:
        return False
    try:
        with open(cmdline_path(pid), "rb") as cmdline_file:
            raw = cmdline_file.read()
    except OSError:
        return False
    argv = [token.decode("utf-8", "replace") for token in raw.split(b"\0") if token]
    return argv == target_argv


def send_signal(pid, sig, task_name=""):
    """send signal to pid. Returns True on success or when the process is
    already gone; False on failure (permission denied or other error).

    task_name is only used for logging context.
    """
    try:
        os.kill(pid, sig)
    except ProcessLookupError:
        return True
    except PermissionError as e:
        logging.error("task %s: permission denied to signal pid %d: %s",
                      task_name, pid, str(e))
        return False
    except OSError as e:
        logging.error("task %s: failed to signal pid %d: %s",
                      task_name, pid, str(e))
        return False
    return True


def wait_exit(pid, target_argv):
    """poll until pid's cmdline no longer matches target_argv or the
    KILL_VERIFY_TIMEOUT deadline is reached.

    Returns True if the process is gone (exited/zombie/recycled),
    False on timeout.
    """
    deadline = time.monotonic() + KILL_VERIFY_TIMEOUT
    while time.monotonic() < deadline:
        if not cmdline_matches(pid, target_argv):
            return True
        time.sleep(KILL_POLL_INTERVAL)
    return False


def kill_and_verify(pid, target_argv, task_name=""):
    """send SIGTERM, verify the process exits, escalate to SIGKILL if needed.

    Returns True if the process is gone; False on failure.

    task_name is only used for logging context.
    """
    if not send_signal(pid, signal.SIGTERM, task_name):
        return False
    if wait_exit(pid, target_argv):
        return True
    logging.warning("task %s: pid %d did not exit after SIGTERM, escalating to SIGKILL",
                    task_name, pid)
    if not send_signal(pid, signal.SIGKILL, task_name):
        return False
    return wait_exit(pid, target_argv)
