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
Description: xalarm transfer
Author:
Create: 2023-11-02
"""

import socket
import logging

USER_RECV_SOCK = "/var/run/xalarm/alarm"
MIN_ID_NUMBER = 1001
MAX_ID_NUMBER = 1128


def check_filter(alarm_info, alarm_filter):
    """check id_mask for alarm messages forwarding
    """
    if not alarm_filter:
        return True
    if alarm_info.alarm_id < MIN_ID_NUMBER or alarm_info.alarm_id > MAX_ID_NUMBER:
        return False
    index = alarm_info.alarm_id - MIN_ID_NUMBER
    if not alarm_filter.id_mask[index]:
        return False
    return True


def transmit_alarm(bin_data):
    """forward alarm message
    """
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        sock.sendto(bin_data, USER_RECV_SOCK)
        logging.debug("transfer alarm success")
    except ConnectionRefusedError:
        logging.debug("transfer sendto failed")
    except FileNotFoundError:
        logging.debug("transfer sendto failed")
    finally:
        sock.close()
