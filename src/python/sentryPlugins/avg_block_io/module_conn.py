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
import json
import logging
import sys
import time

from .utils import is_abnormal, get_win_data, log_slow_win
from sentryCollector.collect_plugin import is_iocollect_valid, get_io_data, Result_Messages
from syssentry.result import ResultLevel, report_result
from xalarm.sentry_notify import xalarm_report, MINOR_ALM, ALARM_TYPE_OCCUR


TASK_NAME = "avg_block_io"

def sig_handler(signum, _f):
    """stop avg_block_io"""
    report_result(TASK_NAME, ResultLevel.PASS, json.dumps({}))
    logging.info("Finished avg_block_io plugin running.")
    sys.exit(0)

def avg_get_io_data(io_dic):
    """get_io_data from sentryCollector"""
    res = get_io_data(io_dic["period_time"], io_dic["disk_list"], io_dic["stage_list"], io_dic["iotype_list"])
    return check_result_validation(res, 'get io data')


def avg_is_iocollect_valid(io_dic, config_disk, config_stage):
    """is_iocollect_valid from sentryCollector"""
    res = is_iocollect_valid(io_dic["period_time"], config_disk, config_stage)
    return check_result_validation(res, 'check config validation')


def check_result_validation(res, reason):
    """check validation of result from sentryCollector"""
    if not 'ret' in res or not 'message' in res:
        err_msg = "Failed to {}: Cannot connect to sentryCollector.".format(reason)
        report_alarm_fail(err_msg)
    if res['ret'] != 0:
        err_msg = "Failed to {}: {}".format(reason, Result_Messages[res['ret']])
        report_alarm_fail(err_msg)

    try:
        json_data = json.loads(res['message'])
    except json.JSONDecodeError:
        err_msg = "Failed to {}: invalid return message".format(reason)
        report_alarm_fail(err_msg)

    return json_data


def report_alarm_fail(alarm_info):
    """report result to xalarmd"""
    report_result(TASK_NAME, ResultLevel.FAIL, json.dumps({"msg": alarm_info}))
    logging.error(alarm_info)
    sys.exit(1)


def process_report_data(disk_name, rw, io_data):
    """check abnormal window and report to xalarm"""
    abnormal, abnormal_list = is_abnormal((disk_name, 'bio', rw), io_data)
    if not abnormal:
        return

    msg = {
        "alarm_source": TASK_NAME, "driver_name": disk_name, "io_type": rw,
        "reason": "unknown", "block_stack": "bio", "alarm_type": abnormal_list,
        "details": get_win_data(disk_name, rw, io_data)
        }

    # io press
    ctrl_stage = ['throtl', 'wbt', 'iocost', 'bfq']
    for stage_name in ctrl_stage:
        abnormal, abnormal_list = is_abnormal((disk_name, 'bio', rw), io_data)
        if not abnormal:
            continue
        msg["reason"] = "IO press"
        msg["block_stack"] = f"bio,{stage_name}"
        msg["alarm_type"] = abnormal_list
        log_slow_win(msg, "IO press")
        xalarm_report(1002, MINOR_ALM, ALARM_TYPE_OCCUR, json.dumps(msg))
        return

    # driver slow
    abnormal, abnormal_list = is_abnormal((disk_name, 'rq_driver', rw), io_data)
    if abnormal:
        msg["reason"] = "driver slow"
        msg["block_stack"] = "bio,rq_driver"
        msg["alarm_type"] = abnormal_list
        log_slow_win(msg, "driver slow")
        xalarm_report(1002, MINOR_ALM, ALARM_TYPE_OCCUR, json.dumps(msg))
        return

    # kernel slow
    kernel_stage = ['gettag', 'plug', 'deadline', 'hctx', 'requeue']
    for stage_name in kernel_stage:
        abnormal, abnormal_list = is_abnormal((disk_name, stage_name, rw), io_data)
        if not abnormal:
            continue
        msg["reason"] = "kernel slow"
        msg["block_stack"] = f"bio,{stage_name}"
        msg["alarm_type"] = abnormal_list
        log_slow_win(msg, "kernel slow")
        xalarm_report(1002, MINOR_ALM, ALARM_TYPE_OCCUR, json.dumps(msg))
        return

    log_slow_win(msg, "unknown")
    xalarm_report(1002, MINOR_ALM, ALARM_TYPE_OCCUR, json.dumps(msg))
