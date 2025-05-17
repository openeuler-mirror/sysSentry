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

import sys
import logging
import json
import signal
import re
import functools
import os

from syssentry.utils import run_cmd, run_popen, get_process_pid
from syssentry.result import ResultLevel, report_result
from syssentry.sentry_config import CpuPluginsParamsConfig, get_log_level

CPU_SENTRY_PARAM_CONFIG = "/etc/sysSentry/plugins/cpu_sentry.ini"
CPU_SENTRY_LOG_FILE = "/var/log/sysSentry/cpu_sentry.log"

# Inspection commands running at the bottom layer
LOW_LEVEL_INSPECT_CMD = "cat-cli"

# max length of msg in details
DETAILS_LOG_MSG_MAX_LEN = 255

class CpuSentry:
    """
    cpu sentry script
    """
    TASK_NAME = "cpu_sentry"

    def __init__(self):
        self.init_send_result()

    @staticmethod
    def cpu_format_convert_to_list(cpu_input):
        if not CpuPluginsParamsConfig.is_valid_cpu_input(cpu_input):
            return []

        cpu_list = []
        cpu_input_list = [cpu_input]
        if "," in cpu_input:
            cpu_input_list = cpu_input.split(",")
        for cpu_input_i in cpu_input_list:
            if type(cpu_input_i) != str:
                return []
            cpu_input_i = cpu_input_i.strip()
            if cpu_input_i.isdigit():
                cpu_list.append(int(cpu_input_i))
            elif re.match(r"^\d+-\d+$", cpu_input_i):
                start, end = map(int, cpu_input_i.split("-"))
                if start > end:
                    logging.error("found invalid format cpu_input %s, start (%d) > end (%s)", cpu_input_i, start, end)
                    return []
                cpu_list.extend([i for i in range(start, end + 1)])
            else:
                logging.error("found invalid format cpu_input %s", cpu_input_i)
        return cpu_list

    @staticmethod
    def get_cpu_list_from_sys_file(source_file="/sys/devices/system/cpu/offline"):
        if not source_file.startswith("/sys/devices/system/cpu") or not os.path.exists(source_file):
            return []
        cpu_list = []
        with open(source_file, "r") as f:
            cpus = f.readlines()
            if len(cpus) == 1:
                cpus = cpus[0]
                if cpus != "\n":
                    cpu_list.extend(CpuSentry.cpu_format_convert_to_list(cpus))
        return cpu_list

    def init_send_result(self):
        """init and clean result info"""
        self.send_result = {
            "task_name": self.TASK_NAME,
            "result": "",
            "details": {
                "code": 0,
                "msg": "",
                "isolated_cpu_list": "",
            }
        }

    def handle_cpu_output(self, stdout: str):
        if not stdout:
            logging.error("%s process output is None, it may be killed!", LOW_LEVEL_INSPECT_CMD)
            self.send_result["result"] = ResultLevel.FAIL
            self.send_result["details"]["code"] = 1005
            self.send_result["details"]["msg"] = "cpu_sentry task is killed!"
            return

        out_split = stdout.split("\n")
        isolated_cores_number = -1
        found_fault_cores_list = []
        error_msg_list = []
        for out_line_i in out_split:
            if "handle_patrol_result: Found fault cores" in out_line_i:
                cores_number_tmp = out_line_i.split("Found fault cores:")[1]
                cores_number_tmp = cores_number_tmp[1:-1]
                logging.info("Found fault cores from cmd out: [%s]", cores_number_tmp)
                found_fault_cores_list.extend(CpuSentry.cpu_format_convert_to_list(cores_number_tmp))
            elif "cpu patrol execute ok, isolated cores:" in out_line_i:
                isolated_cores_number = int(out_line_i.split(':')[1])
            elif out_line_i.startswith('<ISOLATED-CORE-LIST>'):
                self.send_result["details"]["isolated_cpu_list"] = out_line_i.split(':')[1]
                break
            elif "ERROR" in out_line_i:
                logging.error("[cat-cli error] - %s\n", out_line_i)
                error_msg_list.append(out_line_i)

        found_fault_cores_number = len(set(found_fault_cores_list))
        if isolated_cores_number == -1:
            self.send_result["result"] = ResultLevel.FAIL
            self.send_result["details"]["code"] = 1004

            send_error_msg = ""
            # Remove ANSI escape sequences
            for error_info in error_msg_list:
                if error_info.startswith("\u001b"):
                    ansi_escape = r'\x1b\[([0-9]+)(;[0-9]+)*([A-Za-z])'
                    error_info = re.sub(ansi_escape, '', error_info)
                if len(send_error_msg) + len(error_info) < DETAILS_LOG_MSG_MAX_LEN:
                    send_error_msg += ";" + error_info
            self.send_result["details"]["msg"] = send_error_msg
        elif found_fault_cores_number == 0:
            self.send_result["details"]["code"] = 0
            self.send_result["result"] = ResultLevel.PASS
        elif 0 in found_fault_cores_list:
            # Core 0 is a special core and cannot be isolated
            self.send_result["details"]["code"] = 1001
            self.send_result["result"] = ResultLevel.MINOR_ALM
            self.send_result["details"]["msg"] = "Core 0 is a special core and cannot be isolated"
        elif found_fault_cores_number == isolated_cores_number:
            self.send_result["details"]["code"] = 1002
            self.send_result["result"] = ResultLevel.MINOR_ALM
            self.send_result["details"]["msg"] = "Some CPUs are faulty. The faulty cores are isolated successfully."
        else:
            found_fault_cores_set = set(found_fault_cores_list)
            isolated_cpu_set = set(CpuSentry.cpu_format_convert_to_list(self.send_result["details"]["isolated_cpu_list"]))
            self.send_result["details"]["code"] = 1002
            self.send_result["result"] = ResultLevel.MINOR_ALM
            self.send_result["details"]["msg"] = "Some cores are isolated successfully and some cores ({}) fail to be isolated.".format(list(found_fault_cores_set - isolated_cpu_set))

    def cpu_report_result(self):
        """report result to sysSentry"""
        task_name = self.TASK_NAME
        details = self.send_result.get("details", {})
        try:
            details = json.dumps(details)
        except json.decoder.JSONDecodeError:
            logging.error("dumps details failed, don't report result to sysSentry")
            return

        result_level = self.send_result.get("result", ResultLevel.FAIL)
        report_result(task_name, result_level, details)
        self.init_send_result()

def kill_process(signum, _f, cpu_sentry_obj):
    """kill process by 'pkill -9'"""
    run_cmd(f"pkill -9 {LOW_LEVEL_INSPECT_CMD}")
    cpu_sentry_obj.send_result["result"] = ResultLevel.FAIL
    cpu_sentry_obj.send_result["details"]["code"] = 1005
    cpu_sentry_obj.send_result["details"]["msg"] = "cpu_sentry task is killed!"
    cpu_sentry_obj.cpu_report_result()
    sys.exit(1)

def main():
    """main function"""
    log_level = get_log_level(filename=CPU_SENTRY_PARAM_CONFIG)
    log_format = "%(asctime)s - %(levelname)7s - [%(filename)s:%(lineno)d] - %(message)s"
    logging.basicConfig(filename=CPU_SENTRY_LOG_FILE, level=log_level, format=log_format)
    os.chmod(CPU_SENTRY_LOG_FILE, 0o600)

    cpu_sentry_task = CpuSentry()
    cpu_params_config_parser = CpuPluginsParamsConfig(CPU_SENTRY_PARAM_CONFIG, "args")

    signal.signal(signal.SIGINT, functools.partial(kill_process, cpu_sentry_obj=cpu_sentry_task))
    signal.signal(signal.SIGTERM, functools.partial(kill_process, cpu_sentry_obj=cpu_sentry_task))

    # The current program is allowed only once in the same environment
    process_pid = get_process_pid(LOW_LEVEL_INSPECT_CMD)
    if process_pid > -1:
        cpu_sentry_task.send_result["result"] = ResultLevel.SKIP
        logging.warning("An inspection task whose PID is %s already exists.", process_pid)
        cpu_sentry_task.cpu_report_result()
        sys.exit(0)

    try:
        # Parse the configuration file to obtain parameter settings and assemble the command.
        cpu_sentry_cmd_args = cpu_params_config_parser.get_param_dict_from_config()
        cpu_sentry_task_cmd = cpu_params_config_parser.join_cpu_start_cmd(cpu_sentry_cmd_args)

        if cpu_sentry_task_cmd == "":
            cpu_sentry_task.send_result["result"] = ResultLevel.FAIL
            cpu_sentry_task.send_result["details"]["code"] = 1003
            cpu_sentry_task.send_result["details"]["msg"] = "Invalid parameter configuration in cpu_sentry.ini !"
            cpu_sentry_task.cpu_report_result()
            sys.exit(0)

        cpu_task_process_pipe = run_popen(cpu_sentry_task_cmd)
        stdout, _ = cpu_task_process_pipe.communicate()
        logging.debug("task output is as follows: \n %s", stdout)
        cpu_sentry_task.handle_cpu_output(stdout)
    except (FileNotFoundError, IndexError, ValueError):
        cpu_sentry_task.send_result["result"] = ResultLevel.FAIL
        cpu_sentry_task.send_result["details"]["code"] = 1004
        cpu_sentry_task.send_result["details"]["msg"] = "run cmd [%s] raise Error" % cpu_sentry_task_cmd
        cpu_sentry_task.cpu_report_result()
    else:
        cpu_sentry_task.cpu_report_result()
