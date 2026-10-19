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
from datetime import datetime, timezone
import logging
import json
import sys

from xalarm.register_xalarm import xalarm_register,xalarm_getid,xalarm_getlevel,xalarm_gettype,xalarm_gettime,xalarm_getdesc
from xalarm.xalarm_api import Xalarm

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

MILLISECONDS_UNIT_SECONDS = 1000
MAX_NUM_OF_ALARM_ID = 128
MIN_ALARM_ID = 1001
MAX_ALARM_ID = (MIN_ALARM_ID + MAX_NUM_OF_ALARM_ID - 1)
SYSSENTRY_DOWN_ALARM_ID = 1128

ALARM_CLEAR_CHECK_INTERVAL = 60
ALARM_CLEAR_THREAD_JOIN_TIMEOUT = 5
_alarm_clear_stop = threading.Event()
_alarm_clear_thread = None


def _clear_expired_alarms():
    """Background sweep: evict alarms older than alarm_clear_time from all buckets.

    On-insert (update_alarm_list) and on-query (get_alarm_result) clearing
    only run when a new alarm arrives or a get_alarm query is made for a given
    alarm_id. This sweep ensures stale alarms are evicted even when neither
    happens, bounding memory usage by alarm_clear_time.

    The global alarm_list_lock is acquired once per bucket and released
    between buckets, so a concurrent update_alarm_list / get_alarm_result
    caller is only blocked for the bucket currently being swept, not for the
    full sweep. The alarm_list_dict key set is established at alarm_register()
    time and does not change during the sweep, so snapshotting the keys once
    outside the lock is safe. current_time is recomputed per bucket inside
    the lock so the age comparison reflects the moment of eviction rather than
    the moment the sweep started.
    """
    for alarm_id in list(alarm_list_dict.keys()):
        with alarm_list_lock:
            alarm_list = alarm_list_dict.get(alarm_id)
            if not alarm_list:
                continue
            clear_time = alarm_id_clear_time_dict.get(alarm_id, 0)
            if clear_time <= 0:
                continue
            current_time = datetime.now(tz=timezone.utc).timestamp()
            clear_index = -1
            for i, alarm in enumerate(alarm_list):
                if current_time - xalarm_gettime(alarm) / MILLISECONDS_UNIT_SECONDS > clear_time:
                    clear_index = i
                    break
            if clear_index >= 0:
                alarm_list_dict[alarm_id] = alarm_list[:clear_index]
                logging.info(
                    "auto-clear: removed %d expired alarms for alarm_id %d",
                    len(alarm_list) - clear_index, alarm_id
                )


def _alarm_clear_loop():
    while not _alarm_clear_stop.is_set():
        try:
            _clear_expired_alarms()
        except Exception as e:
            logging.error("alarm auto-clear loop error: %s", str(e))
        _alarm_clear_stop.wait(ALARM_CLEAR_CHECK_INTERVAL)


def _start_alarm_clear_thread():
    global _alarm_clear_thread
    if _alarm_clear_thread is not None and _alarm_clear_thread.is_alive():
        return
    _alarm_clear_stop.clear()
    _alarm_clear_thread = threading.Thread(target=_alarm_clear_loop, daemon=True, name="alarm-auto-clear")
    _alarm_clear_thread.start()
    logging.info("alarm auto-clear thread started, interval=%ds", ALARM_CLEAR_CHECK_INTERVAL)


def stop_alarm_clear_thread():
    """Signal the auto-clear thread to exit and wait for it.

    Called from syssentry.py main() finally block on service shutdown so the
    thread exits cleanly instead of being killed by the OS at process exit.
    The thread sleeps via Event.wait(), which returns immediately once the
    stop Event is set, so it normally exits in milliseconds; the join timeout
    is only a safety bound.
    """
    global _alarm_clear_thread
    if _alarm_clear_thread is None:
        return
    if not _alarm_clear_thread.is_alive():
        _alarm_clear_thread = None
        return
    _alarm_clear_stop.set()
    _alarm_clear_thread.join(timeout=ALARM_CLEAR_THREAD_JOIN_TIMEOUT)
    if _alarm_clear_thread.is_alive():
        logging.warning("alarm auto-clear thread did not exit within timeout")
    else:
        logging.info("alarm auto-clear thread stopped")
    _alarm_clear_thread = None


def update_alarm_list(alarm_info: Xalarm):
    alarm_id = xalarm_getid(alarm_info)
    if alarm_id < MIN_ALARM_ID or alarm_id > MAX_ALARM_ID:
        logging.warning(f"Invalid alarm_id {alarm_id}")
        return
    if alarm_id == SYSSENTRY_DOWN_ALARM_ID:
        logging.info("received sysSentry down notification (alarm_id=%d), skip processing", alarm_id)
        return
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

def validate_alarm_id(alarm_id):
    if alarm_id is None:
        return False
    try:
        alarm_id = int(alarm_id)
        if MIN_ALARM_ID <= alarm_id <= MAX_ALARM_ID:
            return True
        else:
            return False
    except ValueError:
        return False

def validate_alarm_clear_time(alarm_clear_time):
    try:
        alarm_clear_time = int(alarm_clear_time)
        if alarm_clear_time > 0 and alarm_clear_time <= sys.maxsize:
            return True
        else:
            return False
    except ValueError:
        return False

def alarm_register():
    logging.debug(f"alarm_register: enter")
    # 初始化告警ID映射字典、告警老化时间字典
    for task_type in TasksMap.tasks_dict:
        for task_name in TasksMap.tasks_dict[task_type]:
            task = TasksMap.tasks_dict[task_type][task_name]
            if not validate_alarm_id(task.alarm_id):
                logging.warning(f"Invalid alarm_id {task.alarm_id}: ignore {task_name} alarm")
                continue
            if not validate_alarm_clear_time(task.alarm_clear_time):
                logging.warning(f"Invalid alarm_clear_time {task.alarm_clear_time}: ignore {task_name} alarm")
                continue
            task.alarm_id = int(task.alarm_id)
            task.alarm_clear_time = int(task.alarm_clear_time)
            alarm_id = task.alarm_id
            alarm_clear_time = task.alarm_clear_time

            alarm_list_dict[alarm_id] = []
            task_alarm_id_dict[task_name] = alarm_id
            if alarm_id not in alarm_id_clear_time_dict:
                alarm_id_clear_time_dict[alarm_id] = alarm_clear_time
            else:
                alarm_id_clear_time_dict[alarm_id] = max(alarm_clear_time, alarm_id_clear_time_dict[alarm_id])
            logging.info(f"alarm_register: {task_name} is registered")
    # 注册告警回调
    id_filter = [True] * 128
    clientId = xalarm_register(update_alarm_list, id_filter)
    if clientId < 0:
        logging.info(f'register xalarm: failed')
        return clientId
    logging.info('register xalarm: success')
    _start_alarm_clear_thread()
    return clientId


def get_alarm_result(task_name: str, time_range: int, detailed: bool):
    """
    get task alarm result

    Returns
    -------
    list or None
        - Return a list of alarm info if get alarm result success
        - Return an empty list [] if alarm info are not found
        - Return None if an exception or error occurred during the operation
    """
    alarm_list_lock.acquire()
    try:
        if task_name not in task_alarm_id_dict:
            logging.debug("task_name does not exist")
            return []
        alarm_id = task_alarm_id_dict[task_name]
        clear_time = alarm_id_clear_time_dict[alarm_id]
        if alarm_id not in alarm_list_dict:
            logging.debug("alarm_id does not exist")
            return []
        alarm_list = alarm_list_dict[alarm_id]
        logging.debug("get_alarm_result: alarm_list of %s has %d elements", alarm_id, len(alarm_list))
        timestamp = datetime.now().timestamp()

        # Evict alarms older than clear_time and persist the trimmed list to
        # the cache, so a query reclaims expired entries instead of relying
        # only on the on-insert path (update_alarm_list). clear_time is the
        # configured cache lifetime (alarm_clear_time).
        clear_index = -1
        for i in range(len(alarm_list)):
            if timestamp - xalarm_gettime(alarm_list[i]) / MILLISECONDS_UNIT_SECONDS > clear_time:
                clear_index = i
                break
        if clear_index >= 0:
            alarm_list = alarm_list[:clear_index]
            alarm_list_dict[alarm_id] = alarm_list

        # Return only alarms within the requested time_range. Query view, NOT
        # persisted: a narrow time_range must not evict alarms that are still
        # within clear_time, otherwise a later wider query would lose them.
        stop_index = -1
        for i in range(len(alarm_list)):
            logging.debug("timestamp, alarm_list[%d].timestamp: %s, %s", i, timestamp, xalarm_gettime(alarm_list[i]))
            if timestamp - xalarm_gettime(alarm_list[i]) / MILLISECONDS_UNIT_SECONDS > time_range:
                stop_index = i
                break
        if stop_index >= 0:
            alarm_list = alarm_list[:stop_index]
        logging.debug("get_alarm_result: final alarm_list of %s has %d elements", alarm_id, len(alarm_list))

        def xalarm_to_dict(alarm_info: Xalarm) -> dict:
            timestamp = alarm_info.timestamp.tv_sec + alarm_info.timestamp.tv_usec / 1000000
            dt_object = datetime.fromtimestamp(int(timestamp))
            return {
                'alarm_id': xalarm_getid(alarm_info),
                'alarm_type': xalarm_gettype(alarm_info),
                'alarm_level': xalarm_getlevel(alarm_info),
                'timestamp': dt_object.strftime("%Y-%m-%d %H:%M:%S"),
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

            # dump each {key,value} of details in one line
            if 'details' in alarm_info and isinstance(alarm_info['details'], dict):
                for key in alarm_info['details']:
                    alarm_info['details'][key] = str(alarm_info['details'][key])

            alarm['alarm_info'] = alarm_info
        alarm_list = [alarm for alarm in alarm_list if 'alarm_source' in alarm['alarm_info'] and alarm['alarm_info']['alarm_source'] == task_name]

        alarm_level_mapping = {
                1: 'MINOR_ALM',
                2: 'MAJOR_ALM',
                3: 'CRITICAL_ALM'
                }

        alarm_type_mapping = {
                1: 'ALARM_TYPE_OCCUR',
                2: 'ALARM_TYPE_RECOVER'
                }

        for alarm in alarm_list:
            alarm['alarm_level'] = alarm_level_mapping.get(alarm['alarm_level'], 'UNKNOWN_LEVEL')
            alarm['alarm_type'] = alarm_type_mapping.get(alarm['alarm_type'], 'UNKNOWN_TYPE')
        return alarm_list

    except json.JSONDecodeError:
        logging.error("get_alarm_result: json decode error")
        return None
    except (IndexError, KeyError, TypeError) as e:
        logging.error("get_alarm_result failed, error msg is: %s", str(e))
        return None
    finally:
        alarm_list_lock.release()
