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
import logging


class ConfigParser:

    DEFAULT_ABSOLUTE_THRESHOLD = 40
    DEFAULT_SLOW_IO_DETECTION_FREQUENCY = 1
    DEFAULT_LOG_LEVEL = 'info'
    DEFAULT_TRAIN_DATA_DURATION = 24
    DEFAULT_TRAIN_UPDATE_DURATION = 2
    DEFAULT_ALGORITHM_TYPE = 'boxplot'
    DEFAULT_N_SIGMA_PARAMETER = 3
    DEFAULT_BOXPLOT_PARAMETER = 1.5
    DEFAULT_SLIDING_WINDOW_TYPE = 'not_continuous'
    DEFAULT_WINDOW_SIZE = 30
    DEFAULT_WINDOW_MINIMUM_THRESHOLD = 6

    def __init__(self, config_file_name):
        self.__boxplot_parameter = None
        self.__window_minimum_threshold = None
        self.__window_size = None
        self.__sliding_window_type = None
        self.__n_sigma_parameter = None
        self.__algorithm_type = None
        self.__train_update_duration = None
        self.__log_level = None
        self.__slow_io_detect_frequency = None
        self.__absolute_threshold = None
        self.__train_data_duration = None
        self.__config_file_name = config_file_name

    def read_config_from_file(self):

        con = configparser.ConfigParser()
        con.read(self.__config_file_name, encoding='utf-8')

        items_common = dict(con.items('common'))
        items_algorithm = dict(con.items('algorithm'))
        items_sliding_window = dict(con.items('sliding_window'))

        try:
            self.__absolute_threshold = int(items_common.get('absolute_threshold',
                                                             ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD))
        except ValueError:
            self.__absolute_threshold = ConfigParser.DEFAULT_ABSOLUTE_THRESHOLD
            logging.warning('absolute threshold type conversion has error, use default value.')

        try:
            self.__slow_io_detect_frequency = int(items_common.get('slow_io_detect_frequency',
                                                                   ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY))
        except ValueError:
            self.__slow_io_detect_frequency = ConfigParser.DEFAULT_SLOW_IO_DETECTION_FREQUENCY
            logging.warning('slow_io_detect_frequency type conversion has error, use default value.')

        self.__log_level = items_common.get('log_level', ConfigParser.DEFAULT_LOG_LEVEL)

        try:
            self.__train_data_duration = float(items_algorithm.get('train_data_duration',
                                                                   ConfigParser.DEFAULT_TRAIN_DATA_DURATION))
        except ValueError:
            self.__train_data_duration = ConfigParser.DEFAULT_TRAIN_DATA_DURATION
            logging.warning('train_data_duration type conversion has error, use default value.')

        try:
            self.__train_update_duration = float(items_algorithm.get('train_update_duration',
                                                                     ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION))
        except ValueError:
            self.__train_update_duration = ConfigParser.DEFAULT_TRAIN_UPDATE_DURATION
            logging.warning('train_update_duration type conversion has error, use default value.')

        try:
            self.__algorithm_type = items_algorithm.get('algorithm_type', ConfigParser.DEFAULT_ALGORITHM_TYPE)
        except ValueError:
            self.__algorithm_type = ConfigParser.DEFAULT_ALGORITHM_TYPE
            logging.warning('algorithmType type conversion has error, use default value.')

        if self.__algorithm_type == 'n_sigma':
            try:
                self.__n_sigma_parameter = float(items_algorithm.get('n_sigma_parameter',
                                                                     ConfigParser.DEFAULT_N_SIGMA_PARAMETER))
            except ValueError:
                self.__n_sigma_parameter = ConfigParser.DEFAULT_N_SIGMA_PARAMETER
                logging.warning('n_sigma_parameter type conversion has error, use default value.')
        elif self.__algorithm_type == 'boxplot':
            try:
                self.__boxplot_parameter = float(items_algorithm.get('boxplot_parameter',
                                                                     ConfigParser.DEFAULT_BOXPLOT_PARAMETER))
            except ValueError:
                self.__boxplot_parameter = ConfigParser.DEFAULT_BOXPLOT_PARAMETER
                logging.warning('boxplot_parameter type conversion has error, use default value.')

        self.__sliding_window_type = items_sliding_window.get('sliding_window_type',
                                                              ConfigParser.DEFAULT_SLIDING_WINDOW_TYPE)

        try:
            self.__window_size = int(items_sliding_window.get('window_size',
                                                              ConfigParser.DEFAULT_WINDOW_SIZE))
        except ValueError:
            self.__window_size = ConfigParser.DEFAULT_WINDOW_SIZE
            logging.warning('window_size type conversion has error, use default value.')

        try:
            self.__window_minimum_threshold = (
                int(items_sliding_window.get('window_minimum_threshold',
                                             ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD)))
        except ValueError:
            self.__window_minimum_threshold = ConfigParser.DEFAULT_WINDOW_MINIMUM_THRESHOLD
            logging.warning('window_minimum_threshold type conversion has error, use default value.')

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
