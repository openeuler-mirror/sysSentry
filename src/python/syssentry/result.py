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
use for report result
"""
import json
import os
import socket
import re
import logging

from enum import Enum


class ResultLevel(Enum):
    """result level for report_result"""
    PASS = 0
    FAIL = 1
    SKIP = 2
    MINOR_ALM = 3
    MAJOR_ALM = 4
    CRITICAL_ALM = 5

# result-specific socket
RESULT_SOCKET_PATH = "/var/run/sysSentry/result.sock"

RESULT_MSG_HEAD_LEN = 10
RESULT_MSG_MAGIC_LEN = 6 # len(RESULT_MAGIC)
RESULT_MAGIC = "RESULT"
RESULT_INFO_MAX_LEN = 4096

RESULT_LEVEL_ERR_MSG_DICT = {
        ResultLevel.PASS.name : "",
        ResultLevel.SKIP.name : "not supported.maybe some rpm package not be installed.",
        ResultLevel.FAIL.name : "FAILED. config may be incorrect or the command may be invalid/killed!",
        ResultLevel.MINOR_ALM.name : "the command output shows that the status is 'INFO' or 'GENERAL_WARN'.",
        ResultLevel.MAJOR_ALM.name : "the command output shows that the status is 'WARN' or 'IMPORTANT_WARN'.",
        ResultLevel.CRITICAL_ALM.name : "the command output shows that the status is 'FAIL' or 'EMERGENCY_WARN'.",
}


def report_result(task_name: str, result_level : ResultLevel, report_data : str) -> int:
    """client socket send and recv message"""
    if not os.path.exists(RESULT_SOCKET_PATH):
        logging.warning("did not send data to sysSentry.")
        return -1

    if not isinstance(task_name, str) \
            or not isinstance(result_level, ResultLevel) \
            or not isinstance(report_data, str):
        logging.error("params type is wrong!")
        return -1

    pattern_name = r"^[a-zA-Z][a-zA-Z0-9_]*$"
    if not re.match(pattern_name, task_name):
        logging.error("'task_name' (%s) contains illegal characters.", task_name)
        return -1

    send_data_dict = {
        "task_name" : task_name,
        "result_data": {
            "result" : result_level.name,
            "details": report_data
        }
    }
    try:
        send_data_str = json.dumps(send_data_dict)
    except json.decoder.JSONDecodeError:
        logging.error("failed dump data to json!")
        return -1

    req_data_len = len(send_data_str)
    if req_data_len > RESULT_INFO_MAX_LEN:
        logging.error("send data len is too long %d > %d)!", req_data_len, RESULT_INFO_MAX_LEN)
        return -1

    try:
        client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    except socket.error:
        logging.error("sentryctl: client creat socket error")
        return -1

    try:
        client_socket.connect(RESULT_SOCKET_PATH)
    except OSError:
        client_socket.close()
        logging.error("sentryctl: client connect error")
        return -1

    request_msg = RESULT_MAGIC + str(req_data_len).zfill(RESULT_MSG_HEAD_LEN-RESULT_MSG_MAGIC_LEN) + send_data_str
    request_msg = request_msg.encode()

    try:
        client_socket.sendall(request_msg)
        server_reponse_info = client_socket.recv(len("SUCCESS"))
        server_reponse_info = server_reponse_info.decode().strip()
        if server_reponse_info != "SUCCESS":
            logging.error("report result to sysSentry, get incorrect response information : %s", server_reponse_info)
    except (OSError, UnicodeError):
        client_socket.close()
        logging.error("sentryctl: client communicate error")
        return -1
    client_socket.close()
    return 0
