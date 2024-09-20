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

import time
import signal
import logging

from .detector import Detector
from .threshold import ThresholdFactory, AbsoluteThreshold
from .sliding_window import SlidingWindowFactory
from .utils import (get_threshold_type_enum, get_sliding_window_type_enum, get_data_queue_size_and_update_size,
                   get_log_level)
from .config_parser import ConfigParser
from .data_access import get_io_data_from_collect_plug, check_collect_valid
from .io_data import MetricName
from .alarm_report import AlarmReport

CONFIG_FILE = "/etc/sysSentry/plugins/ai_threshold_slow_io_detection.ini"


def sig_handler(signum, frame):
    logging.info("receive signal: %d", signum)
    AlarmReport().report_fail(f"receive signal: {signum}")
    exit(signum)


class SlowIODetection:
    _config_parser = None
    _disk_list = None
    _detector_name_list = []
    _detectors = {}

    def __init__(self, config_parser: ConfigParser):
        self._config_parser = config_parser
        self.__set_log_format()
        self.__init_detector_name_list()
        self.__init_detector()

    def __set_log_format(self):
        log_format = "%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s"
        log_level = get_log_level(self._config_parser.get_log_level())
        logging.basicConfig(level=log_level, format=log_format)

    def __init_detector_name_list(self):
        self._disk_list = check_collect_valid(self._config_parser.get_slow_io_detect_frequency())
        for disk in self._disk_list:
            self._detector_name_list.append(MetricName(disk, "bio", "read", "latency"))
            self._detector_name_list.append(MetricName(disk, "bio", "write", "latency"))

    def __init_detector(self):
        train_data_duration, train_update_duration = (self._config_parser.
                                                      get_train_data_duration_and_train_update_duration())
        slow_io_detection_frequency = self._config_parser.get_slow_io_detect_frequency()
        threshold_type = get_threshold_type_enum(self._config_parser.get_algorithm_type())
        data_queue_size, update_size = get_data_queue_size_and_update_size(train_data_duration,
                                                                           train_update_duration,
                                                                           slow_io_detection_frequency)
        sliding_window_type = get_sliding_window_type_enum(self._config_parser.get_sliding_window_type())
        window_size, window_threshold = self._config_parser.get_window_size_and_window_minimum_threshold()

        for detector_name in self._detector_name_list:
            threshold = ThresholdFactory().get_threshold(threshold_type, data_queue_size=data_queue_size,
                                                         data_queue_update_size=update_size)
            sliding_window = SlidingWindowFactory().get_sliding_window(sliding_window_type, queue_length=window_size,
                                                                       threshold=window_threshold)
            detector = Detector(detector_name, threshold, sliding_window)
            # 绝对阈值的阈值初始化
            if isinstance(threshold, AbsoluteThreshold):
                threshold.set_threshold(self._config_parser.get_absolute_threshold())
            self._detectors[detector_name] = detector
            logging.info(f"add detector: {detector}")

    def launch(self):
        while True:
            logging.debug('step0. AI threshold slow io event detection is looping.')

            # Step1：获取IO数据
            io_data_dict_with_disk_name = get_io_data_from_collect_plug(
                self._config_parser.get_slow_io_detect_frequency(), self._disk_list
            )
            logging.debug(f'step1. Get io data: {str(io_data_dict_with_disk_name)}')
            if io_data_dict_with_disk_name is None:
                continue
            # Step2：慢IO检测
            logging.debug('step2. Start to detection slow io event.')
            slow_io_event_list = []
            for metric_name, detector in self._detectors.items():
                result = detector.is_slow_io_event(io_data_dict_with_disk_name)
                if result[0]:
                    slow_io_event_list.append((detector.get_metric_name(), result))
            logging.debug('step2. End to detection slow io event.')

            # Step3：慢IO事件上报
            logging.debug('step3. Report slow io event to sysSentry.')
            for slow_io_event in slow_io_event_list:
                metric_name: MetricName = slow_io_event[0]
                result = slow_io_event[1]
                AlarmReport.report_major_alm(f"disk {metric_name.get_disk_name()} has slow io event."
                                             f"stage: {metric_name.get_metric_name()},"
                                             f"type: {metric_name.get_io_access_type_name()},"
                                             f"metric: {metric_name.get_metric_name()},"
                                             f"current window: {result[1]},"
                                             f"threshold: {result[2]}")
                logging.error(f"slow io event happen: {str(slow_io_event)}")

            # Step4：等待检测时间
            logging.debug('step4. Wait to start next slow io event detection loop.')
            time.sleep(self._config_parser.get_slow_io_detect_frequency())


def main():
    # Step1：注册消息处理函数
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
    # Step2：断点恢复
    # todo:

    # Step3：读取配置
    config_file_name = CONFIG_FILE
    config = ConfigParser(config_file_name)
    config.read_config_from_file()

    # Step4：启动慢IO检测
    slow_io_detection = SlowIODetection(config)
    slow_io_detection.launch()
