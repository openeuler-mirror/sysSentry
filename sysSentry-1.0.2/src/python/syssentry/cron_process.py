# coding: utf-8
# Copyright (c) 2023 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

"""
period task.
"""
import io
import os
import time
import logging
import subprocess

from .global_values import InspectTask
from .task_map import TasksMap, PERIOD_TYPE
from .mod_status import set_runtime_status, WAITING_STATUS, RUNNING_STATUS, \
    FAILED_STATUS, EXITED_STATUS


class PeriodTask(InspectTask):
    """period task class"""
    def __init__(self, name: str, task_type: str, task_start: str, task_stop: str, interval):
        super().__init__(name, task_type, task_start, task_stop)
        self.interval = int(interval)
        self.last_exec_timestamp = 0
        self.runtime_status = WAITING_STATUS

    def stop(self):
        self.period_enabled = False
        cmd_list = self.task_stop.split()
        try:
            subprocess.Popen(cmd_list, stdout=subprocess.PIPE, close_fds=True)
        except OSError:
            logging.error("task stop Popen failed, invalid cmd")
        if self.runtime_status != RUNNING_STATUS:
            self.runtime_status = EXITED_STATUS

    def start(self):
        if not self.period_enabled:
            self.period_enabled = True
            self.upgrade_period_timestamp()

        cmd_list = self.task_start.split()
        try:
            logfile = open(self.log_file, 'a')
            os.chmod(self.log_file, 0o600)
        except OSError:
            logging.error("task %s log_file %s open failed", self.name, self.log_file)
            logfile = subprocess.PIPE
        try:
            child = subprocess.Popen(cmd_list, stdout=logfile,
                                     stderr=subprocess.STDOUT, close_fds=True)
        except OSError:
            logging.error("period task %s start Popen failed", self.name)
            self.runtime_status = FAILED_STATUS
            return False, "period task start popen failed, invalid command"
        finally:
            if isinstance(logfile, io.TextIOWrapper) and not logfile.closed:
                logfile.close()

        self.pid = child.pid
        logging.debug("start task %s pid %d", self.name, self.pid)
        self.runtime_status = RUNNING_STATUS
        if self.heartbeat_interval > 0:
            self.last_heartbeat = time.perf_counter()
        return True, ""

    def check_period_timestamp(self):
        """check timestamp for period mod"""
        if self.last_exec_timestamp == 0:
            logging.debug("first time exec this task")
            return True
        cur_timestamp = time.perf_counter()
        if cur_timestamp >= (self.interval + self.last_exec_timestamp):
            logging.debug("check period time success")
            return True
        return False

    def upgrade_period_timestamp(self):
        """upgrade timestamp for period mod"""
        cur_timestamp = time.perf_counter()

        self.last_exec_timestamp = cur_timestamp
        logging.debug("period current timestamp %d", self.last_exec_timestamp)


def period_tasks_handle():
    """period tasks handler"""
    period_list = TasksMap.tasks_dict.get(PERIOD_TYPE)
    for task_name in period_list:
        task = period_list.get(task_name)
        if not task.load_enabled:
            continue

        if not task.period_enabled:
            logging.debug("period not enabled")
            continue

        if task.runtime_status == WAITING_STATUS and \
                task.check_period_timestamp():
            logging.info("period task exec! name: %s", task.name)
            res, _ = task.start()
            if res:
                set_runtime_status(task.name, RUNNING_STATUS)
            task.upgrade_period_timestamp()
