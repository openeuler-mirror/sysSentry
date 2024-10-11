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

import os
import configparser
import logging

from .alarm_report import Report
from .threshold import ThresholdType
from .utils import get_threshold_type_enum, get_sliding_window_type_enum, get_log_level


LOG_FORMAT = "%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s"

ALL_STAGE_LIST = ['throtl', 'wbt', 'gettag', 'plug', 'deadline', 'hctx', 'requeue', 'rq_driver', 'bio']
ALL_IOTPYE_LIST = ['read', 'write']


def init_log_format(log_level: str):
    logging.basicConfig(level=get_log_level(log_level.lower()), format=LOG_FORMAT)
    if log_level.lower() not in ("info", "warning", "error", "debug"):
        logging.warning(
            f"the log_level: {log_level} you set is invalid, use default value: info."
        )


class ConfigParser:
    DEFAULT_ABSOLUTE_THRESHOLD = 40
    DEFAULT_SLOW_IO_DETECTION_FREQUENCY = 1
    DEFAULT_LOG_LEVEL = "info"

    DEFAULT_STAGE = 'throtl,wbt,gettag,plug,deadline,hctx,requeue,rq_driver,bio'
    DEFAULT_IOTYPE = 'read,write'

    DEFAULT_ALGORITHM_TYPE = "boxplot"
    DEFAULT_TRAIN_DATA_DURATION = 24
    DEFAULT_TRAIN_UPDATE_DURATION = 2
    DEFAULT_BOXPLOT_PARAMETER = 1.5
    DEFAULT_N_SIGMA_PARAMETER = 3

    DEFAULT_SLIDING_WINDOW_TYPE = "not_continuous"
    DEFAULT_WINDOW_SIZE = 30
    DEFAULT_WINDOW_MINIMUM_THRESHOLD = 6

    def __init__(self, config_file_name):
        self.__absolute_threshold = ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD
        self.__slow_io_detect_frequency = (
            ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY
        )
        self.__log_level = ConfigParser.DEFAULT_LOG_LEVEL
        self.__disks_to_detection = None
        self.__stage = ConfigParser.DEFAULT_STAGE
        self.__iotype = ConfigParser.DEFAULT_IOTYPE

        self.__algorithm_type = get_threshold_type_enum(
            ConfigParser.DEFAULT_ALGORITHM_TYPE
        )
        self.__train_data_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        self.__train_update_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        self.__boxplot_parameter = ConfigParser.DEFAULT_BOXPLOT_PARAMETER
        self.__n_sigma_parameter = ConfigParser.DEFAULT_N_SIGMA_PARAMETER

        self.__sliding_window_type = ConfigParser.DEFAULT_SLIDING_WINDOW_TYPE
        self.__window_size = ConfigParser.DEFAULT_WINDOW_SIZE
        self.__window_minimum_threshold = ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD

        self.__config_file_name = config_file_name

    def _get_config_value(
        self,
        config_items: dict,
        key: str,
        value_type,
        default_value=None,
        gt=None,
        ge=None,
        lt=None,
        le=None,
    ):
        value = config_items.get(key)
        if value is None:
            logging.warning(
                "config of %s not found, the default value %s will be used.",
                key,
                default_value,
            )
            value = default_value
        if not value:
            logging.critical(
                "the value of %s is empty, ai_block_io plug will exit.", key
            )
            Report.report_pass(
                f"the value of {key} is empty, ai_block_io plug will exit."
            )
            exit(1)
        try:
            value = value_type(value)
        except ValueError:
            logging.critical(
                "the value of %s is not a valid %s, ai_block_io plug will exit.",
                key,
                value_type,
            )
            Report.report_pass(
                f"the value of {key} is not a valid {value_type}, ai_block_io plug will exit."
            )
            exit(1)
        if gt is not None and value <= gt:
            logging.critical(
                "the value of %s is not greater than %s, ai_block_io plug will exit.",
                key,
                gt,
            )
            Report.report_pass(
                f"the value of {key} is not greater than {gt}, ai_block_io plug will exit."
            )
            exit(1)
        if ge is not None and value < ge:
            logging.critical(
                "the value of %s is not greater than or equal to %s, ai_block_io plug will exit.",
                key,
                ge,
            )
            Report.report_pass(
                f"the value of {key} is not greater than or equal to {ge}, ai_block_io plug will exit."
            )
            exit(1)
        if lt is not None and value >= lt:
            logging.critical(
                "the value of %s is not less than %s, ai_block_io plug will exit.",
                key,
                lt,
            )
            Report.report_pass(
                f"the value of {key} is not less than {lt}, ai_block_io plug will exit."
            )
            exit(1)
        if le is not None and value > le:
            logging.critical(
                "the value of %s is not less than or equal to %s, ai_block_io plug will exit.",
                key,
                le,
            )
            Report.report_pass(
                f"the value of {key} is not less than or equal to {le}, ai_block_io plug will exit."
            )
            exit(1)

        return value

    def __read_absolute_threshold(self, items_common: dict):
        self.__absolute_threshold = self._get_config_value(
            items_common,
            "absolute_threshold",
            float,
            ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD,
            gt=0,
        )

    def __read__slow_io_detect_frequency(self, items_common: dict):
        self.__slow_io_detect_frequency = self._get_config_value(
            items_common,
            "slow_io_detect_frequency",
            int,
            ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY,
            gt=0,
            le=300,
        )

    def __read__disks_to_detect(self, items_common: dict):
        disks_to_detection = items_common.get("disk")
        if disks_to_detection is None:
            logging.warning("config of disk not found, the default value will be used.")
            self.__disks_to_detection = None
            return
        disks_to_detection = disks_to_detection.strip()
        if not disks_to_detection:
            logging.critical("the value of disk is empty, ai_block_io plug will exit.")
            Report.report_pass(
                "the value of disk is empty, ai_block_io plug will exit."
            )
            exit(1)
        disk_list = disks_to_detection.split(",")
        if len(disk_list) == 1 and disk_list[0] == "default":
            self.__disks_to_detection = None
            return
        self.__disks_to_detection = disk_list

    def __read__train_data_duration(self, items_algorithm: dict):
        self.__train_data_duration = self._get_config_value(
            items_algorithm,
            "train_data_duration",
            float,
            ConfigParser.DEFAULT_TRAIN_DATA_DURATION,
            gt=0,
            le=720,
        )

    def __read__train_update_duration(self, items_algorithm: dict):
        default_train_update_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        if default_train_update_duration > self.__train_data_duration:
            default_train_update_duration = self.__train_data_duration / 2
        self.__train_update_duration = self._get_config_value(
            items_algorithm,
            "train_update_duration",
            float,
            default_train_update_duration,
            gt=0,
            le=self.__train_data_duration,
        )

    def __read__algorithm_type_and_parameter(self, items_algorithm: dict):
        algorithm_type = items_algorithm.get(
            "algorithm_type", ConfigParser.DEFAULT_ALGORITHM_TYPE
        )
        self.__algorithm_type = get_threshold_type_enum(algorithm_type)
        if self.__algorithm_type is None:
            logging.critical(
                "the algorithm_type: %s you set is invalid. ai_block_io plug will exit.",
                algorithm_type,
            )
            Report.report_pass(
                f"the algorithm_type: {algorithm_type} you set is invalid. ai_block_io plug will exit."
            )
            exit(1)

        if self.__algorithm_type == ThresholdType.NSigmaThreshold:
            self.__n_sigma_parameter = self._get_config_value(
                items_algorithm,
                "n_sigma_parameter",
                float,
                ConfigParser.DEFAULT_N_SIGMA_PARAMETER,
                gt=0,
                le=10,
            )
        elif self.__algorithm_type == ThresholdType.BoxplotThreshold:
            self.__boxplot_parameter = self._get_config_value(
                items_algorithm,
                "boxplot_parameter",
                float,
                ConfigParser.DEFAULT_BOXPLOT_PARAMETER,
                gt=0,
                le=10,
            )

    def __read__stage(self, items_algorithm: dict):
        stage_str = items_algorithm.get('stage', ConfigParser.DEFAULT_STAGE)
        stage_list = stage_str.split(',')
        if len(stage_list) == 1 and stage_list[0] == '':
            logging.critical('stage value not allow is empty, exiting...')
            exit(1)
        if len(stage_list) == 1 and stage_list[0] == 'default':
            logging.warning(f'stage will enable default value: {ConfigParser.DEFAULT_STAGE}')
            self.__stage = ALL_STAGE_LIST
            return
        for stage in stage_list:
            if stage not in ALL_STAGE_LIST:
                logging.critical(f'stage: {stage} is not valid stage, ai_block_io will exit...')
                exit(1)
        dup_stage_list = set(stage_list)
        if 'bio' not in dup_stage_list:
            logging.critical('stage must contains bio stage, exiting...')
            exit(1)
        self.__stage = dup_stage_list

    def __read__iotype(self,  items_algorithm: dict):
        iotype_str = items_algorithm.get('iotype', ConfigParser.DEFAULT_IOTYPE)
        iotype_list = iotype_str.split(',')
        if len(iotype_list) == 1 and iotype_list[0] == '':
            logging.critical('iotype value not allow is empty, exiting...')
            exit(1)
        if len(iotype_list) == 1 and iotype_list[0] == 'default':
            logging.warning(f'iotype will enable default value: {ConfigParser.DEFAULT_IOTYPE}')
            self.__iotype = ALL_IOTPYE_LIST
            return
        for iotype in iotype_list:
            if iotype not in ALL_IOTPYE_LIST:
                logging.critical(f'iotype: {iotype} is not valid iotype, ai_block_io will exit...')
                exit(1)
        dup_iotype_list = set(iotype_list)
        self.__iotype = dup_iotype_list

    def __read__window_size(self, items_sliding_window: dict):
        self.__window_size = self._get_config_value(
            items_sliding_window,
            "window_size",
            int,
            ConfigParser.DEFAULT_WINDOW_SIZE,
            gt=0,
            le=3600,
        )

    def __read__window_minimum_threshold(self, items_sliding_window: dict):
        default_window_minimum_threshold = ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD
        if default_window_minimum_threshold > self.__window_size:
            default_window_minimum_threshold = self.__window_size / 2
        self.__window_minimum_threshold = self._get_config_value(
            items_sliding_window,
            "window_minimum_threshold",
            int,
            default_window_minimum_threshold,
            gt=0,
            le=self.__window_size,
        )

    def read_config_from_file(self):
        if not os.path.exists(self.__config_file_name):
            init_log_format(self.__log_level)
            logging.critical(
                "config file %s not found, ai_block_io plug will exit.",
                self.__config_file_name,
            )
            Report.report_pass(
                f"config file {self.__config_file_name} not found, ai_block_io plug will exit."
            )
            exit(1)

        con = configparser.ConfigParser()
        try:
            con.read(self.__config_file_name, encoding="utf-8")
        except configparser.Error as e:
            init_log_format(self.__log_level)
            logging.critical(
                f"config file read error: %s, ai_block_io plug will exit.", e
            )
            Report.report_pass(
                f"config file read error: {e}, ai_block_io plug will exit."
            )
            exit(1)

        if con.has_section('log'):
            items_log = dict(con.items('log'))
            # 情况一：没有log，则使用默认值
            # 情况二：有log，值为空或异常，使用默认值
            # 情况三：有log，值正常，则使用该值
            self.__log_level = items_log.get('level', ConfigParser.DEFAULT_LOG_LEVEL)
            init_log_format(self.__log_level)
        else:
            init_log_format(self.__log_level)
            logging.warning(f"log section parameter not found, it will be set to default value.")

        if con.has_section("common"):
            items_common = dict(con.items("common"))
            self.__read_absolute_threshold(items_common)
            self.__read__slow_io_detect_frequency(items_common)
            self.__read__disks_to_detect(items_common)
            self.__read__stage(items_common)
            self.__read__iotype(items_common)
        else:
            logging.warning(
                "common section parameter not found, it will be set to default value."
            )

        if con.has_section("algorithm"):
            items_algorithm = dict(con.items("algorithm"))
            self.__read__train_data_duration(items_algorithm)
            self.__read__train_update_duration(items_algorithm)
            self.__read__algorithm_type_and_parameter(items_algorithm)
        else:
            logging.warning(
                "algorithm section parameter not found, it will be set to default value."
            )

        if con.has_section("sliding_window"):
            items_sliding_window = dict(con.items("sliding_window"))
            sliding_window_type = items_sliding_window.get(
                "sliding_window_type", ConfigParser.DEFAULT_SLIDING_WINDOW_TYPE
            )
            self.__sliding_window_type = get_sliding_window_type_enum(
                sliding_window_type
            )
            self.__read__window_size(items_sliding_window)
            self.__read__window_minimum_threshold(items_sliding_window)
        else:
            logging.warning(
                "sliding_window section parameter not found, it will be set to default value."
            )

        self.__print_all_config_value()

    def __repr__(self):
        config_str = {
            'log.level': self.__log_level,
            'common.absolute_threshold': self.__absolute_threshold,
            'common.slow_io_detect_frequency': self.__slow_io_detect_frequency,
            'common.disk': self.__disks_to_detection,
            'common.stage': self.__stage,
            'common.iotype': self.__iotype,
            'algorithm.train_data_duration': self.__train_data_duration,
            'algorithm.train_update_duration': self.__train_update_duration,
            'algorithm.algorithm_type': self.__algorithm_type,
            'algorithm.boxplot_parameter': self.__boxplot_parameter,
            'algorithm.n_sigma_parameter': self.__n_sigma_parameter,
            'sliding_window.sliding_window_type': self.__sliding_window_type,
            'sliding_window.window_size': self.__window_size,
            'sliding_window.window_minimum_threshold': self.__window_minimum_threshold
        }
        return str(config_str)

    def __print_all_config_value(self):
        logging.info(f"all config is follow:\n {self}")

    def get_train_data_duration_and_train_update_duration(self):
        return self.__train_data_duration, self.__train_update_duration

    def get_window_size_and_window_minimum_threshold(self):
        return self.__window_size, self.__window_minimum_threshold

    @property
    def slow_io_detect_frequency(self):
        return self.__slow_io_detect_frequency

    @property
    def algorithm_type(self):
        return self.__algorithm_type

    @property
    def sliding_window_type(self):
        return self.__sliding_window_type

    @property
    def train_data_duration(self):
        return self.__train_data_duration

    @property
    def train_update_duration(self):
        return self.__train_update_duration

    @property
    def window_size(self):
        return self.__window_size

    @property
    def window_minimum_threshold(self):
        return self.__window_minimum_threshold

    @property
    def absolute_threshold(self):
        return self.__absolute_threshold

    @property
    def log_level(self):
        return self.__log_level

    @property
    def disks_to_detection(self):
        return self.__disks_to_detection

    @property
    def stage(self):
        return self.__stage

    @property
    def iotype(self):
        return self.__iotype

    @property
    def boxplot_parameter(self):
        return self.__boxplot_parameter

    @property
    def n_sigma_parameter(self):
        return self.__n_sigma_parameter
