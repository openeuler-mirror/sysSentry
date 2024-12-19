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
Description: xalarm config
Author:
Create: 2023-11-02
"""

import re
import dataclasses
import logging
from configparser import ConfigParser


MAIN_CONFIG_PATH = '/etc/sysSentry/xalarm.conf'
ALARM_ID_PATTERN = r'^\d+$'
ALARM_ID_RANGE_PATTERN = r'^\d+-\d+$'
MIN_ID_NUMBER = 1001
MAX_ID_NUMBER = 1128
MAX_ID_MASK_CAPACITY = 128


@dataclasses.dataclass
class AlarmConfig:
    """alarm config
    """
    def __init__(self):
        self.id_mask = [True for i in range(MAX_ID_MASK_CAPACITY)]
        self.transfer_list = []


def _is_id(item):
    """
    check alarm id, an alarm id should in range (1001, 1128)
    """
    if not re.match(ALARM_ID_PATTERN, item):
        return False
    return True


def _is_id_range(item):
    """
    check alarm id range(eg: begin-end)
    """
    if not re.match(ALARM_ID_RANGE_PATTERN, item):
        return False
    return True


def _parse_id_mask(raw_id_mask):
    """parse id_mask in config
    """
    id_mask = [False for i in range(MAX_ID_MASK_CAPACITY)]

    item_list = raw_id_mask.strip().split(',')
    for item in item_list:
        item = item.strip()
        if _is_id(item):
            tmp_id = int(item)
            if tmp_id < MIN_ID_NUMBER or tmp_id > MAX_ID_NUMBER:
                logging.info("invalid alarm id %s, ignored", item)
                continue
            id_mask[tmp_id - MIN_ID_NUMBER] = True
            continue
        if _is_id_range(item):
            begin, end = item.split('-')
            begin = MIN_ID_NUMBER if int(begin) < MIN_ID_NUMBER else int(begin)
            end = MAX_ID_NUMBER if int(end) > MAX_ID_NUMBER else int(end)
            if end < begin:
                logging.info("invalid alarm id %s, ignored", item)
                continue
            for i in range(begin, end + 1):
                id_mask[i - MIN_ID_NUMBER] = True
            continue
        logging.info("invalid alarm id %s, ignored", item)
    return id_mask


def parse_id_mask(cfg):
    """parse id_mask in config
    """
    if 'filter' not in cfg.sections():
        logging.info("no filter conf")
        return None

    filter_conf = cfg['filter']
    if 'id_mask' not in filter_conf:
        logging.info("no id_mask conf")
        return None
    raw_id_mask = filter_conf['id_mask']

    id_mask = _parse_id_mask(raw_id_mask)
    return id_mask


def config_init():
    """config initialize
    """
    alarm_config = AlarmConfig()

    cfg = ConfigParser()
    cfg.read(MAIN_CONFIG_PATH)

    id_mask = parse_id_mask(cfg)
    if id_mask:
        alarm_config.id_mask = id_mask

    return alarm_config
