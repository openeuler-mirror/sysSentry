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
use for report alarm
"""
import threading
from typing import Dict, List
from datetime import datetime
import time
import logging
import json

from xalarm.register_xalarm import xalarm_register,xalarm_getid,xalarm_getlevel,xalarm_gettype,xalarm_gettime,xalarm_getdesc
from xalarm.xalarm_api import Xalarm

from .global_values import InspectTask
from .task_map import TasksMap

# 告警ID映射字典，key为插件名，value为告警ID（类型为数字）
task_alarm_id_dict: Dict[str, int] = {}

# 告警老化时间字典，key为告警ID，value为老化时间（类型为数字，单位为秒）
alarm_id_clear_time_dict: Dict[int, int] = {}

# 告警事件列表，key为告警ID，value为告警ID对应的告警事件列表（类型为list）
alarm_list_dict: Dict[int, List[Xalarm]] = {}
# 告警事件列表锁
alarm_list_lock = threading.Lock()

id_filter = []
id_base = 1001
clientId = -1

MILLISECONDS_UNIT_SECONDS = 1000

def update_alarm_list(alarm_info: Xalarm):
    alarm_id = xalarm_getid(alarm_info)
    timestamp = xalarm_gettime(alarm_info)
    if not timestamp:
        logging.error("Retrieve timestamp failed")
        return
    alarm_list_lock.acquire()
    try:
        # new alarm is inserted into list head
        if alarm_id not in alarm_list_dict:
            logging.warning(f"update_alarm_list: alarm_id {alarm_id} not found in alarm_list_dict")
            return
        alarm_list = alarm_list_dict[alarm_id]

        alarm_list.insert(0, alarm_info)
        # clear alarm_info older than clear time threshold
        clear_index = -1
        clear_time = alarm_id_clear_time_dict[alarm_id]
        for i in range(len(alarm_list)):
            if (timestamp - xalarm_gettime(alarm_list[i])) / MILLISECONDS_UNIT_SECONDS > clear_time:
                clear_index = i
                break
        if clear_index >= 0:
            alarm_list_dict[alarm_id] = alarm_list[:clear_index]
    finally:
        alarm_list_lock.release()

def alarm_register():
    logging.debug(f"alarm_register: enter")
    # 初始化告警ID映射字典、告警老化时间字典
    for task_type in TasksMap.tasks_dict:
        for task_name in TasksMap.tasks_dict[task_type]:
            logging.info(f"alarm_register: {task_name} is registered")
            task = TasksMap.tasks_dict[task_type][task_name]
            alarm_id = task.alarm_id
            alarm_clear_time = task.alarm_clear_time
            alarm_list_dict[alarm_id] = []
            task_alarm_id_dict[task_name] = alarm_id
            if alarm_id not in alarm_id_clear_time_dict:
                alarm_id_clear_time_dict[alarm_id] = alarm_clear_time
            else:
                alarm_id_clear_time_dict[alarm_id] = max(alarm_clear_time, alarm_id_clear_time_dict[alarm_id])
    # 注册告警回调
    id_filter = [True] * 128
    clientId = xalarm_register(update_alarm_list, id_filter)
    if clientId < 0:
        logging.info(f'register xalarm: failed')
        return clientId
    logging.info('register xalarm: success')
    return clientId

def get_alarm_result(task_name: str, time_range: int, detailed: bool) -> List[Dict]:
    alarm_list_lock.acquire()
    try:
        if task_name not in task_alarm_id_dict:
            logging.debug("task_name does not exist")
            return []
        alarm_id = task_alarm_id_dict[task_name]
        if alarm_id not in alarm_list_dict:
            logging.debug("alarm_id does not exist")
            return []
        alarm_list = alarm_list_dict[alarm_id]
        logging.debug(f"get_alarm_result: alarm_list of {alarm_id} has {len(alarm_list)} elements")
        # clear alarm_info older than clear time threshold
        stop_index = -1
        timestamp = int(datetime.now().timestamp())
        for i in range(len(alarm_list)):
            logging.debug(f"timestamp, alarm_list[{i}].timestamp: {timestamp}, {xalarm_gettime(alarm_list[i])}")
            if timestamp - (xalarm_gettime(alarm_list[i])) / MILLISECONDS_UNIT_SECONDS > int(time_range):
                stop_index = i
                break
        if stop_index >= 0:
            alarm_list = alarm_list[:stop_index]
        logging.debug(f"get_alarm_result: final alarm_list of {alarm_id} has {len(alarm_list)} elements")

        def xalarm_to_dict(alarm_info: Xalarm) -> dict:
            return {
                'alarm_id': xalarm_getid(alarm_info),
                'alarm_type': xalarm_gettype(alarm_info),
                'alarm_level': xalarm_getlevel(alarm_info),
                'timetamp': xalarm_gettime(alarm_info),
                'msg1': xalarm_getdesc(alarm_info)
            }

        alarm_list = [xalarm_to_dict(alarm) for alarm in alarm_list]

        # keep detail
        for alarm in alarm_list:
            alarm_info = alarm['msg1']
            alarm_info = json.loads(alarm_info)
            if not detailed:
                if 'details' in alarm_info:
                    alarm_info.pop('details', None)
            alarm.pop('msg1', None)
            alarm['alarm_info'] = alarm_info
        return alarm_list
    finally:
        alarm_list_lock.release()
