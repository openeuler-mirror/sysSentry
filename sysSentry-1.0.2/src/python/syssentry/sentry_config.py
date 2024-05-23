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
Read and save inspect.conf value.
"""
import configparser
import logging


DEFAULT_INSPECT_DELAY = 3
INSPECT_CONF_PATH = "/etc/sysSentry/inspect.conf"


class SentryConfig:
    """
    SentryConfig class, saving inspect.conf value
    """
    inspect_delay = -1

    @staticmethod
    def init_param(conf_path=INSPECT_CONF_PATH):
        """initialize reading params from inspect.conf"""
        SentryConfig.inspect_delay = DEFAULT_INSPECT_DELAY

        try:
            inspect_conf = configparser.ConfigParser()
            inspect_conf.read(conf_path)
        except configparser.Error:
            logging.error("inspect configure file read failed")
            return False

        if SentryConfig.check_conf(inspect_conf):
            try:
                insp_delay = int(inspect_conf['inspect']['Interval'])
            except ValueError:
                logging.error("inspect delay value not valid")
                return False
            logging.debug("inspect delay is %d", insp_delay)
            SentryConfig.inspect_delay = insp_delay
            return True
        return False

    @staticmethod
    def check_conf(inspect_conf):
        """check if conf valid"""
        if 'inspect' not in inspect_conf:
            logging.debug("inspect config not exist")
            return False
        if 'Interval' not in inspect_conf['inspect']:
            logging.debug("Interval conf not exist")
            return False
        return True
