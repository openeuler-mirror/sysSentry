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
tasks map class and initialize function.
"""
import logging
from typing import Dict

ONESHOT_TYPE = "ONESHOT"
PERIOD_TYPE = "PERIOD"

TASKS_MAP = None

class TasksMap:
    """task map class"""
    tasks_dict: Dict[str, Dict] = {}

    @classmethod
    def init_task_map(cls):
        """initialize TaskMap"""
        cls.tasks_dict.update({ONESHOT_TYPE: {}, PERIOD_TYPE: {}})

    @classmethod
    def add_task(cls, task, task_type, task_name):
        """add new task to tasks_dict"""
        if task_type.upper() != PERIOD_TYPE and task_type.upper() != ONESHOT_TYPE:
            return False
        cls.tasks_dict.get(task_type).update({task_name: task})
        return True

    @classmethod
    def is_task_exist(cls, task_name):
        """check if task_name in tasks_dict"""
        if task_name not in cls.tasks_dict.get(ONESHOT_TYPE) and \
            task_name not in cls.tasks_dict.get(PERIOD_TYPE):
            return False
        return True

    @classmethod
    def get_task_type(cls, task_name):
        """get task type by task_name"""
        task_type = ""
        if task_name in cls.tasks_dict.get(PERIOD_TYPE):
            return PERIOD_TYPE
        if task_name in cls.tasks_dict.get(ONESHOT_TYPE):
            return ONESHOT_TYPE
        return task_type

    @classmethod
    def get_task_by_name(cls, task_name):
        """get task by task_name"""
        res = None
        for task_type in cls.tasks_dict:
            if task_name in cls.tasks_dict.get(task_type):
                res = cls.tasks_dict.get(task_type).get(task_name)
                logging.debug("getting task by name: %s", res)
                break
        return res

