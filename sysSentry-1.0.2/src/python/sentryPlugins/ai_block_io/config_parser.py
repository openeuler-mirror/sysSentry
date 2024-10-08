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

import configparser
import json
import logging

from .io_data import MetricName
from .threshold import ThresholdType
from .utils import get_threshold_type_enum, get_sliding_window_type_enum, get_log_level

LOG_FORMAT = "%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s"


def init_log_format(log_level: str):
    logging.basicConfig(level=get_log_level(log_level.lower()), format=LOG_FORMAT)
    if log_level.lower() not in ('info', 'warning', 'error', 'debug'):
        logging.warning(f'the log_level: {log_level} you set is invalid, use default value: info.')


class ConfigParser:
    DEFAULT_ABSOLUTE_THRESHOLD = 40
    DEFAULT_SLOW_IO_DETECTION_FREQUENCY = 1
    DEFAULT_LOG_LEVEL = 'info'

    DEFAULT_ALGORITHM_TYPE = 'boxplot'
    DEFAULT_TRAIN_DATA_DURATION = 24
    DEFAULT_TRAIN_UPDATE_DURATION = 2
    DEFAULT_BOXPLOT_PARAMETER = 1.5
    DEFAULT_N_SIGMA_PARAMETER = 3

    DEFAULT_SLIDING_WINDOW_TYPE = 'not_continuous'
    DEFAULT_WINDOW_SIZE = 30
    DEFAULT_WINDOW_MINIMUM_THRESHOLD = 6

    def __init__(self, config_file_name):
        self.__absolute_threshold = ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD
        self.__slow_io_detect_frequency = ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY
        self.__log_level = ConfigParser.DEFAULT_LOG_LEVEL
        self.__disks_to_detection: list = []

        self.__algorithm_type = ConfigParser.DEFAULT_ALGORITHM_TYPE
        self.__train_data_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        self.__train_update_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        self.__boxplot_parameter = ConfigParser.DEFAULT_BOXPLOT_PARAMETER
        self.__n_sigma_parameter = ConfigParser.DEFAULT_N_SIGMA_PARAMETER

        self.__sliding_window_type = ConfigParser.DEFAULT_SLIDING_WINDOW_TYPE
        self.__window_size = ConfigParser.DEFAULT_WINDOW_SIZE
        self.__window_minimum_threshold = ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD

        self.__config_file_name = config_file_name

    def __read_absolute_threshold(self, items_common: dict):
        try:
            self.__absolute_threshold = float(items_common.get('absolute_threshold',
                                                               ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD))
            if self.__absolute_threshold <= 0:
                logging.warning(
                    f'the_absolute_threshold: {self.__absolute_threshold} you set is invalid, use default value: {ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD}.')
                self.__absolute_threshold = ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD
        except ValueError:
            self.__absolute_threshold = ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD
            logging.warning(
                f'the_absolute_threshold type conversion has error, use default value: {self.__absolute_threshold}.')

    def __read__slow_io_detect_frequency(self, items_common: dict):
        try:
            self.__slow_io_detect_frequency = int(items_common.get('slow_io_detect_frequency',
                                                                   ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY))
            if self.__slow_io_detect_frequency < 1 or self.__slow_io_detect_frequency > 10:
                logging.warning(
                    f'the slow_io_detect_frequency: {self.__slow_io_detect_frequency} you set is invalid, use default value: {ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY}.')
                self.__slow_io_detect_frequency = ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY
        except ValueError:
            self.__slow_io_detect_frequency = ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY
            logging.warning(f'slow_io_detect_frequency type conversion has error, use default value: {self.__slow_io_detect_frequency}.')

    def __read__disks_to_detect(self, items_common: dict):
        disks_to_detection = items_common.get('disks_to_detect')
        if disks_to_detection is None:
            logging.warning(f'config of disks_to_detect not found, the default value be used.')
            self.__disks_to_detection = None
            return
        try:
            disks_to_detection_list = json.loads(disks_to_detection)
            for disk_to_detection in disks_to_detection_list:
                disk_name = disk_to_detection.get('disk_name', None)
                stage_name = disk_to_detection.get('stage_name', None)
                io_access_type_name = disk_to_detection.get('io_access_type_name', None)
                metric_name = disk_to_detection.get('metric_name', None)
                if not (disk_name is None or stage_name is None or io_access_type_name is None or metric_name is None):
                    metric_name_object = MetricName(disk_name, stage_name, io_access_type_name, metric_name)
                    self.__disks_to_detection.append(metric_name_object)
                else:
                    logging.warning(f'config of disks_to_detect\'s some part has some error: {disk_to_detection}, it will be ignored.')
        except json.decoder.JSONDecodeError as e:
            logging.warning(f'config of disks_to_detect is error: {e}, it will be ignored and default value be used.')
            self.__disks_to_detection = None

    def __read__train_data_duration(self, items_algorithm: dict):
        try:
            self.__train_data_duration = float(items_algorithm.get('train_data_duration',
                                                                   ConfigParser.DEFAULT_TRAIN_DATA_DURATION))
            if self.__train_data_duration <= 0 or self.__train_data_duration > 720:
                logging.warning(
                    f'the train_data_duration: {self.__train_data_duration} you set is invalid, use default value: {ConfigParser.DEFAULT_TRAIN_DATA_DURATION}.')
                self.__train_data_duration = ConfigParser.DEFAULT_TRAIN_DATA_DURATION
        except ValueError:
            self.__train_data_duration = ConfigParser.DEFAULT_TRAIN_DATA_DURATION
            logging.warning(f'the train_data_duration type conversion has error, use default value: {self.__train_data_duration}.')

    def __read__train_update_duration(self, items_algorithm: dict):
        default_train_update_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
        if default_train_update_duration > self.__train_data_duration:
            default_train_update_duration = self.__train_data_duration / 2

        try:
            self.__train_update_duration = float(items_algorithm.get('train_update_duration',
                                                                     ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION))
            if self.__train_update_duration <= 0 or self.__train_update_duration > self.__train_data_duration:
                logging.warning(
                    f'the train_update_duration: {self.__train_update_duration} you set is invalid, use default value: {default_train_update_duration}.')
                self.__train_update_duration = default_train_update_duration
        except ValueError:
            self.__train_update_duration = default_train_update_duration
            logging.warning(f'the train_update_duration type conversion has error, use default value: {self.__train_update_duration}.')

    def __read__algorithm_type_and_parameter(self, items_algorithm: dict):
        algorithm_type = items_algorithm.get('algorithm_type', ConfigParser.DEFAULT_ALGORITHM_TYPE)
        self.__algorithm_type = get_threshold_type_enum(algorithm_type)

        if self.__algorithm_type == ThresholdType.NSigmaThreshold:
            try:
                self.__n_sigma_parameter = float(items_algorithm.get('n_sigma_parameter',
                                                                     ConfigParser.DEFAULT_N_SIGMA_PARAMETER))
                if self.__n_sigma_parameter <= 0 or self.__n_sigma_parameter > 10:
                    logging.warning(
                        f'the n_sigma_parameter: {self.__n_sigma_parameter} you set is invalid, use default value: {ConfigParser.DEFAULT_N_SIGMA_PARAMETER}.')
                    self.__n_sigma_parameter = ConfigParser.DEFAULT_N_SIGMA_PARAMETER
            except ValueError:
                self.__n_sigma_parameter = ConfigParser.DEFAULT_N_SIGMA_PARAMETER
                logging.warning(f'the n_sigma_parameter type conversion has error, use default value: {self.__n_sigma_parameter}.')
        elif self.__algorithm_type == ThresholdType.BoxplotThreshold:
            try:
                self.__boxplot_parameter = float(items_algorithm.get('boxplot_parameter',
                                                                     ConfigParser.DEFAULT_BOXPLOT_PARAMETER))
                if self.__boxplot_parameter <= 0 or self.__boxplot_parameter > 10:
                    logging.warning(
                        f'the boxplot_parameter: {self.__boxplot_parameter} you set is invalid, use default value: {ConfigParser.DEFAULT_BOXPLOT_PARAMETER}.')
                    self.__n_sigma_parameter = ConfigParser.DEFAULT_BOXPLOT_PARAMETER
            except ValueError:
                self.__boxplot_parameter = ConfigParser.DEFAULT_BOXPLOT_PARAMETER
                logging.warning(f'the boxplot_parameter type conversion has error, use default value: {self.__boxplot_parameter}.')

    def __read__window_size(self, items_sliding_window: dict):
        try:
            self.__window_size = int(items_sliding_window.get('window_size',
                                                              ConfigParser.DEFAULT_WINDOW_SIZE))
            if self.__window_size < 1 or self.__window_size > 3600:
                logging.warning(
                    f'the window_size: {self.__window_size} you set is invalid, use default value: {ConfigParser.DEFAULT_WINDOW_SIZE}.')
                self.__window_size = ConfigParser.DEFAULT_WINDOW_SIZE
        except ValueError:
            self.__window_size = ConfigParser.DEFAULT_WINDOW_SIZE
            logging.warning(f'window_size type conversion has error, use default value: {self.__window_size}.')

    def __read__window_minimum_threshold(self, items_sliding_window: dict):
        default_window_minimum_threshold = ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD
        if default_window_minimum_threshold > self.__window_size:
            default_window_minimum_threshold = self.__window_size / 2
        try:
            self.__window_minimum_threshold = (
                int(items_sliding_window.get('window_minimum_threshold',
                                             ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD)))
            if self.__window_minimum_threshold < 1 or self.__window_minimum_threshold > self.__window_size:
                logging.warning(
                    f'the window_minimum_threshold: {self.__window_minimum_threshold} you set is invalid, use default value: {default_window_minimum_threshold}.')
                self.__window_minimum_threshold = default_window_minimum_threshold
        except ValueError:
            self.__window_minimum_threshold = default_window_minimum_threshold
            logging.warning(f'window_minimum_threshold type conversion has error, use default value: {self.__window_minimum_threshold}.')

    def read_config_from_file(self):
        con = configparser.ConfigParser()
        try:
            con.read(self.__config_file_name, encoding='utf-8')
        except configparser.Error as e:
            logging.warning(f'config file read error: {e}, use default value.')
            con = configparser.ConfigParser()
            
        if con.has_section('common'):
            items_common = dict(con.items('common'))
            self.__log_level = items_common.get('log_level', ConfigParser.DEFAULT_LOG_LEVEL)
            init_log_format(self.__log_level)
            self.__read_absolute_threshold(items_common)
            self.__read__slow_io_detect_frequency(items_common)
            self.__read__disks_to_detect(items_common)
        else:
            init_log_format(self.__log_level)
            logging.warning("common section parameter not found, it will be set to default value.")

        if con.has_section('algorithm'):
            items_algorithm = dict(con.items('algorithm'))
            self.__read__train_data_duration(items_algorithm)
            self.__read__train_update_duration(items_algorithm)
            self.__read__algorithm_type_and_parameter(items_algorithm)
        else:
            logging.warning("algorithm section parameter not found, it will be set to default value.")

        if con.has_section('sliding_window'):
            items_sliding_window = dict(con.items('sliding_window'))
            sliding_window_type = items_sliding_window.get('sliding_window_type',
                                                           ConfigParser.DEFAULT_SLIDING_WINDOW_TYPE)
            self.__sliding_window_type = get_sliding_window_type_enum(sliding_window_type)
            self.__read__window_size(items_sliding_window)
            self.__read__window_minimum_threshold(items_sliding_window)
        else:
            logging.warning("sliding_window section parameter not found, it will be set to default value.")

        self.__print_all_config_value()

    def __print_all_config_value(self):
        pass

    def get_slow_io_detect_frequency(self):
        return self.__slow_io_detect_frequency

    def get_algorithm_type(self):
        return self.__algorithm_type

    def get_sliding_window_type(self):
        return self.__sliding_window_type

    def get_train_data_duration_and_train_update_duration(self):
        return self.__train_data_duration, self.__train_update_duration

    def get_window_size_and_window_minimum_threshold(self):
        return self.__window_size, self.__window_minimum_threshold

    def get_absolute_threshold(self):
        return self.__absolute_threshold

    def get_log_level(self):
        return self.__log_level

    def get_disks_to_detection(self):
        return self.__disks_to_detection

    def get_boxplot_parameter(self):
        return self.__boxplot_parameter

    def get_n_sigma_parameter(self):
        return self.__n_sigma_parameter
