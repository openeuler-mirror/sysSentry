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
from collections import defaultdict

from .detector import Detector, DiskDetector
from .threshold import ThresholdFactory, ThresholdType
from .sliding_window import SlidingWindowFactory
from .utils import get_data_queue_size_and_update_size
from .config_parser import ConfigParser
from .data_access import (
    get_io_data_from_collect_plug,
    check_collect_valid,
    get_disk_type,
)
from .io_data import MetricName
from .alarm_report import Xalarm, Report

CONFIG_FILE = "/etc/sysSentry/plugins/ai_block_io.ini"


def sig_handler(signum, frame):
    logging.info("receive signal: %d", signum)
    Report.report_pass(f"receive signal: {signum}, exiting...")
    exit(signum)


class SlowIODetection:
    _config_parser = None
    _disk_list = None
    _detector_name_list = defaultdict(list)
    _disk_detectors = {}

    def __init__(self, config_parser: ConfigParser):
        self._config_parser = config_parser
        self.__init_detector_name_list()
        self.__init_detector()

    def __init_detector_name_list(self):
        self._disk_list = check_collect_valid(
            self._config_parser.period_time
        )
        if self._disk_list is None:
            Report.report_pass(
                "get available disk error, please check if the collector plug is enable. exiting..."
            )
            logging.critical("get available disk error, please check if the collector plug is enable. exiting...")
            exit(1)

        logging.info(f"ai_block_io plug has found disks: {self._disk_list}")
        disks: list = self._config_parser.disks_to_detection
        stages: list = self._config_parser.stage
        iotypes: list = self._config_parser.iotype
        # 情况1：None，则启用所有磁盘检测
        # 情况2：is not None and len = 0，则不启动任何磁盘检测
        # 情况3：len ！= 0，则取交集
        if disks is None:
            logging.warning(
                "you not specify any disk or use default, so ai_block_io will enable all available disk."
            )
        for disk in self._disk_list:
            if disks is not None:
                if disk not in disks:
                    continue
                disks.remove(disk)

            disk_type_result = get_disk_type(disk)
            if disk_type_result["ret"] == 0 and disk_type_result["message"] in (
                '0',
                '1',
                '2',
            ):
                disk_type = int(disk_type_result["message"])
            else:
                logging.warning(
                    "%s get disk type error, return %s, so it will be ignored.",
                    disk,
                    disk_type_result,
                )
                continue
            for stage in stages:
                for iotype in iotypes:
                    self._detector_name_list[disk].append(MetricName(disk, disk_type, stage, iotype, "latency"))
                    self._detector_name_list[disk].append(MetricName(disk, disk_type, stage, iotype, "io_dump"))
        if disks:
            logging.warning(
                "disks: %s not in available disk list, so they will be ignored.",
                disks,
            )
        if not self._detector_name_list:
            logging.critical("the disks to detection is empty, ai_block_io will exit.")
            Report.report_pass(
                "the disks to detection is empty, ai_block_io will exit."
            )
            exit(1)

    def __init_detector(self):
        train_data_duration, train_update_duration = (
            self._config_parser.get_train_data_duration_and_train_update_duration()
        )
        slow_io_detection_frequency = self._config_parser.period_time
        threshold_type = self._config_parser.algorithm_type
        data_queue_size, update_size = get_data_queue_size_and_update_size(
            train_data_duration, train_update_duration, slow_io_detection_frequency
        )
        sliding_window_type = self._config_parser.sliding_window_type
        window_size, window_threshold = (
            self._config_parser.get_window_size_and_window_minimum_threshold()
        )

        for disk, metric_name_list in self._detector_name_list.items():
            disk_detector = DiskDetector(disk)
            for metric_name in metric_name_list:

                if metric_name.metric_name == 'latency':
                    threshold = ThresholdFactory().get_threshold(
                        threshold_type,
                        boxplot_parameter=self._config_parser.boxplot_parameter,
                        n_sigma_paramter=self._config_parser.n_sigma_parameter,
                        data_queue_size=data_queue_size,
                        data_queue_update_size=update_size,
                    )
                    tot_lim = self._config_parser.get_tot_lim(
                        metric_name.disk_type, metric_name.io_access_type_name
                    )
                    avg_lim = self._config_parser.get_avg_lim(
                        metric_name.disk_type, metric_name.io_access_type_name
                    )
                    if tot_lim is None:
                        logging.warning(
                            "disk %s, disk type %s, io type %s, get tot lim error, so it will be ignored.",
                            disk,
                            metric_name.disk_type,
                            metric_name.io_access_type_name,
                        )
                    sliding_window = SlidingWindowFactory().get_sliding_window(
                        sliding_window_type,
                        queue_length=window_size,
                        threshold=window_threshold,
                        abs_threshold=tot_lim,
                        avg_lim=avg_lim
                    )
                    detector = Detector(metric_name, threshold, sliding_window)
                    disk_detector.add_detector(detector)
                    continue

                elif metric_name.metric_name == 'io_dump':
                    threshold = ThresholdFactory().get_threshold(ThresholdType.AbsoluteThreshold)
                    abs_threshold = None
                    if metric_name.io_access_type_name == 'read':
                        abs_threshold = self._config_parser.read_iodump_lim
                    elif metric_name.io_access_type_name == 'write':
                        abs_threshold = self._config_parser.write_iodump_lim
                    sliding_window = SlidingWindowFactory().get_sliding_window(
                        sliding_window_type,
                        queue_length=window_size,
                        threshold=window_threshold
                    )
                    detector = Detector(metric_name, threshold, sliding_window)
                    threshold.set_threshold(abs_threshold)
                    disk_detector.add_detector(detector)

            logging.info(f"disk: [{disk}] add detector:\n [{disk_detector}]")
            self._disk_detectors[disk] = disk_detector

    def launch(self):
        while True:
            logging.debug("step0. AI threshold slow io event detection is looping.")

            # Step1：获取IO数据
            io_data_dict_with_disk_name = get_io_data_from_collect_plug(
                self._config_parser.period_time, self._disk_list
            )
            logging.debug(f"step1. Get io data: {str(io_data_dict_with_disk_name)}")
            if io_data_dict_with_disk_name is None:
                Report.report_pass(
                    "get io data error, please check if the collector plug is enable. exitting..."
                )
                exit(1)

            # Step2：慢IO检测
            logging.debug("step2. Start to detection slow io event.")
            slow_io_event_list = []
            for disk, disk_detector in self._disk_detectors.items():
                result = disk_detector.is_slow_io_event(io_data_dict_with_disk_name)
                if result[0]:
                    slow_io_event_list.append(result)
            logging.debug("step2. End to detection slow io event.")

            # Step3：慢IO事件上报
            logging.debug("step3. Report slow io event to sysSentry.")
            for slow_io_event in slow_io_event_list:
                alarm_content = {
                    "driver_name": slow_io_event[1],
                    "reason": slow_io_event[2],
                    "block_stack": slow_io_event[3],
                    "io_type": slow_io_event[4],
                    "alarm_source": "ai_block_io",
                    "alarm_type": slow_io_event[5],
                    "details": slow_io_event[6],
                }
                Xalarm.major(alarm_content)
                logging.warning("[SLOW IO] " + str(alarm_content))

            # Step4：等待检测时间
            logging.debug("step4. Wait to start next slow io event detection loop.")
            time.sleep(self._config_parser.period_time)


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
