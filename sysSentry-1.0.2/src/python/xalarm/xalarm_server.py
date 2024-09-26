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
Description: xalarm server
Author:
Create: 2023-11-02
"""

import socket
import os
import logging
import select
import threading
from struct import error as StructParseError

from .xalarm_api import alarm_bin2stu
from .xalarm_transfer import check_filter, transmit_alarm, wait_for_connection


ALARM_DIR = "/var/run/xalarm"
USER_RECV_SOCK = "/var/run/xalarm/alarm"
SOCK_FILE = "/var/run/xalarm/report"
ALARM_REPORT_LEN = 1048
ALARM_DIR_PERMISSION = 0o750
ALARM_LISTEN_QUEUE_LEN = 5


def clear_sock_path():
    """unlink unix socket if exist
    """
    if not os.path.exists(ALARM_DIR):
        os.mkdir(ALARM_DIR)
        os.chmod(ALARM_DIR, ALARM_DIR_PERMISSION)
    if os.path.exists(SOCK_FILE):
        os.unlink(SOCK_FILE)
    if os.path.exists(USER_RECV_SOCK):
        os.unlink(USER_RECV_SOCK)


def server_loop(alarm_config):
    """alarm daemon process loop
    """
    logging.info("server loop waiting for messages")
    clear_sock_path()

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind(SOCK_FILE)
    os.chmod(SOCK_FILE, 0o600)

    alarm_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    alarm_sock.bind(USER_RECV_SOCK)
    os.chmod(USER_RECV_SOCK, 0o600)
    alarm_sock.listen(ALARM_LISTEN_QUEUE_LEN)
    alarm_sock.setblocking(False)

    epoll = select.epoll()
    epoll.register(alarm_sock.fileno(), select.EPOLLIN)
    fd_to_socket = {alarm_sock.fileno(): alarm_sock,}
    thread_should_stop = False

    thread = threading.Thread(target=wait_for_connection, args=(alarm_sock, epoll, fd_to_socket, thread_should_stop))
    thread.daemon = True
    thread.start()

    while True:
        try:
            data, _ = sock.recvfrom(ALARM_REPORT_LEN)
            if not data:
                continue
            if len(data) != ALARM_REPORT_LEN:
                logging.debug("server receive report msg length wrong %d",
                                len(data))
                continue
            alarm_info = alarm_bin2stu(data)
            logging.debug("server bin2stu msg")
            if not check_filter(alarm_info, alarm_config):
                continue
            transmit_alarm(alarm_sock, epoll, fd_to_socket, data)
        except Exception as e:
            logging.error(f"Error server:{e}")
            
    thread_should_stop = True
    thread.join()

    epoll.unregister(alarm_sock.fileno())
    epoll.close()
    alarm_sock.close()
    os.unlink(USER_RECV_SOCK)

    sock.close()

