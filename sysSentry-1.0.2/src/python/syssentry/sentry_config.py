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
import os
import re
import sys

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


class CpuPluginsParamsConfig:
    """
    PluginsConfig class, write plugin config (eg./etc/sysSentry/plugins/cpu_sentry.ini)
    and read plugin config
    """

    # Inspection commands running at the bottom layer
    LOW_LEVEL_INSPECT_CMD = "cat-cli"

    def __init__(self, config_file_path="/etc/sysSentry/plugins/cpu_sentry.ini", param_section_name="args"):
        self.config_file = config_file_path
        self.param_section_name = param_section_name
        self.config = configparser.ConfigParser(allow_no_value=True, comment_prefixes=('#', ';'))

    @staticmethod
    def get_cpu_info():
        with open("/sys/devices/system/cpu/present", "r") as f:
            cpu_list = f.readline()
        return cpu_list.strip()

    @staticmethod
    def is_valid_cpu_input(cpu_list: str) -> bool:
        '''
        we only check whether the input meets the format requirements and does
        not check whether the specific cpuid exceeds the maximum CPU core ID.
        input sample :
        Valid input  :  1    1-10    1-10,21,24,35-38
        Invalid input:  -1   1-      1-10,21,24;-38  1-10-20 1--29
        '''
        if cpu_list.isdigit():
            return True
        pattern = r"^(\d+(?:-\d+)?,?)*\d+(?:-\d+)?$"
        if re.match(pattern, cpu_list):
            return True
        return False

    def get_param_dict_from_config(self):
        """read config file"""
        config_param_section_args = {}
        if os.path.exists(self.config_file):
            try:
                self.config.read(self.config_file)
                config_param_section_args = dict(self.config[self.param_section_name])
            except (ValueError, KeyError, configparser.InterpolationSyntaxError):
                config_param_section_args = {}
                logging.error("Failed to parse cpu_sentry.ini!")
        return config_param_section_args

    def join_cpu_start_cmd(self, cpu_param_dict: dict) -> str:
        if not cpu_param_dict:
            return ""

        cpu_list = cpu_param_dict.get("cpu_list", "default")
        if cpu_list == "default":
            cpu_list = CpuPluginsParamsConfig.get_cpu_info()
        elif not CpuPluginsParamsConfig.is_valid_cpu_input(cpu_list):
            logging.error("config 'cpu_list' (value [%s]) is invalid in cpu_sentry.ini !", cpu_list)
            return ""
        patrol_second = cpu_param_dict.get("patrol_second", "60")
        if not patrol_second.isdigit():
            logging.error("config 'patrol_second' (value [%s]) is invalid in cpu_sentry.ini !", patrol_second)
            return ""
        elif patrol_second == "0" or int(patrol_second) > sys.maxsize:
            logging.error("config 'patrol_second' (value [%s]) is invalid in cpu_sentry.ini !", patrol_second)
            return ""
        cpu_utility = cpu_param_dict.get("cpu_utility", "100")
        if not cpu_utility.isdigit():
            logging.error("config 'cpu_utility' (value [%s]) is invalid in cpu_sentry.ini !", cpu_utility)
            return ""
        elif cpu_utility == "0" or int(cpu_utility) > 100:
            logging.error("config 'cpu_utility' (value [%s]) is invalid in cpu_sentry.ini !", cpu_utility)
            return ""
        return "{} -m 0x0001 -l {} -t {} -u {}".format(self.LOW_LEVEL_INSPECT_CMD, cpu_list, patrol_second, cpu_utility)
