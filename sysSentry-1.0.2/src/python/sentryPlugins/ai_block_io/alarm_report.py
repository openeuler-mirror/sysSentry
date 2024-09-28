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

from syssentry.result import ResultLevel, report_result
import logging
import json


class AlarmReport:
    TASK_NAME = "ai_block_io"

    @staticmethod
    def report_pass(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.PASS, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} PASS: {info}')

    @staticmethod
    def report_fail(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.FAIL, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} FAIL: {info}')

    @staticmethod
    def report_skip(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.SKIP, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} SKIP: {info}')

    @staticmethod
    def report_minor_alm(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.MINOR_ALM, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} MINOR_ALM: {info}')

    @staticmethod
    def report_major_alm(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.MAJOR_ALM, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} MAJOR_ALM: {info}')

    @staticmethod
    def report_critical_alm(info: str):
        report_result(AlarmReport.TASK_NAME, ResultLevel.CRITICAL_ALM, json.dumps({"msg": info}))
        logging.info(f'Report {AlarmReport.TASK_NAME} CRITICAL_ALM: {info}')

