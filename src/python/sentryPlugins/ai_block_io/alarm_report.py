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

import logging
import json

from xalarm.sentry_notify import (
    xalarm_report,
    MINOR_ALM,
    MAJOR_ALM,
    CRITICAL_ALM,
    ALARM_TYPE_OCCUR,
    ALARM_TYPE_RECOVER,
)

from syssentry.result import ResultLevel, report_result


class Report:
    TASK_NAME = "ai_block_io"

    @staticmethod
    def report_pass(info: str):
        report_result(Report.TASK_NAME, ResultLevel.PASS, json.dumps({"msg": info}))
        logging.debug(f'Report {Report.TASK_NAME} PASS: {info}')

    @staticmethod
    def report_fail(info: str):
        report_result(Report.TASK_NAME, ResultLevel.FAIL, json.dumps({"msg": info}))
        logging.debug(f'Report {Report.TASK_NAME} FAIL: {info}')

    @staticmethod
    def report_skip(info: str):
        report_result(Report.TASK_NAME, ResultLevel.SKIP, json.dumps({"msg": info}))
        logging.debug(f'Report {Report.TASK_NAME} SKIP: {info}')


class Xalarm:
    ALARM_ID = 1002

    @staticmethod
    def minor(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, MINOR_ALM, ALARM_TYPE_OCCUR, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} MINOR_ALM: {info_str}")

    @staticmethod
    def major(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, MAJOR_ALM, ALARM_TYPE_OCCUR, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} MAJOR_ALM: {info_str}")

    @staticmethod
    def critical(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, CRITICAL_ALM, ALARM_TYPE_OCCUR, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} CRITICAL_ALM: {info_str}")

    def minor_recover(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, MINOR_ALM, ALARM_TYPE_RECOVER, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} MINOR_ALM Recover: {info_str}")

    def major_recover(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, MAJOR_ALM, ALARM_TYPE_RECOVER, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} MAJOR_ALM Recover: {info_str}")

    def critical_recover(info: dict):
        info_str = json.dumps(info)
        xalarm_report(Xalarm.ALARM_ID, CRITICAL_ALM, ALARM_TYPE_RECOVER, info_str)
        logging.debug(f"Report {Xalarm.ALARM_ID} CRITICAL_ALM Recover: {info_str}")
