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
Description: xalarm api
Author:
Create: 2023-11-02
"""
import dataclasses
import struct


ALARM_TYPES = (0, 1, 2)
ALARM_LEVELS = (1, 2, 3, 4, 5)
ALARM_SOCK_PATH = "/var/run/xalarm/report"
MIN_ALARM_ID = 1001
MAX_ALARM_ID = 1128
MAX_MSG_LEN = 8192


@dataclasses.dataclass
class TimevalStu:
    """Time value
    """
    def __init__(self, tv_sec, tv_usec):
        self.tv_sec = tv_sec
        self.tv_usec = tv_usec


class Xalarm:
    """Xalarm
    """
    def __init__(self, alarm_id, alarm_type, alarm_level,
                 tv_sec, tv_usec, msg1):
        self._alarm_id = alarm_id
        self._alarm_type = alarm_type
        self._alarm_level = alarm_level
        self.timetamp = TimevalStu(tv_sec, tv_usec)
        self._msg1 = msg1

    @property
    def alarm_id(self):
        """alarm_id property
        """
        return self._alarm_id

    @alarm_id.setter
    def alarm_id(self, value):
        """alarm_id setter
        """
        if value < MIN_ALARM_ID or value > MAX_ALARM_ID:
            raise ValueError("alarm_id must between {} and {}".format(MIN_ALARM_ID, MAX_ALARM_ID))
        self._alarm_id = value

    @property
    def alarm_type(self):
        """alarm_type property
        """
        return self._alarm_type

    @alarm_type.setter
    def alarm_type(self, value):
        """alarm_type setter
        """
        if value not in ALARM_TYPES:
            raise ValueError("alarm_type must be normal, abnormal or event")
        self._alarm_type = value

    @property
    def alarm_level(self):
        """alarm_level property
        """
        return self._alarm_level

    @alarm_level.setter
    def alarm_level(self, value):
        """alarm_level setter
        """
        if value not in ALARM_LEVELS:
            raise ValueError("alarm_level must be 1, 2, 3, 4, 5")
        self._alarm_level = value

    @property
    def msg1(self):
        """msg1 property
        """
        return self._msg1

    @msg1.setter
    def msg1(self, msg):
        """msg1 setter
        """
        if len(msg) > 512:
            raise ValueError("msg1 length must below 512")
        self._msg1 = msg


def alarm_bin2stu(bin_data):
    """alarm binary to struct
    """
    struct_data = struct.unpack("@HBBll512s", bin_data)

    alarm_info = Xalarm(1001, 2, 1, 0, 0, "")
    alarm_info.alarm_id = struct_data[0]
    alarm_info.alarm_level = struct_data[1]
    alarm_info.alarm_type = struct_data[2]
    alarm_info.timetamp.tv_sec = struct_data[3]
    alarm_info.timetamp.tv_usec = struct_data[4]
    alarm_info.msg1 = struct_data[5]

    return alarm_info


def alarm_stu2bin(alarm_info: Xalarm):
    alarm_msg = alarm_info.msg1
    padding_length = MAX_MSG_LEN - len(alarm_msg)
    if padding_length > 0:
        alarm_msg = alarm_msg + ('\x00' * padding_length)
    return struct.pack(
        f'@HBBll{MAX_MSG_LEN}s',
        alarm_info.alarm_id,
        alarm_info.alarm_level,
        alarm_info.alarm_type,
        alarm_info.timetamp.tv_sec,
        alarm_info.timetamp.tv_usec,
        alarm_msg.encode('utf-8'))
