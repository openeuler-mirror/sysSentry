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
import logging

from .io_data import MetricName
from .threshold import Threshold
from .sliding_window import SlidingWindow
from .utils import get_metric_value_from_io_data_dict_by_metric_name


class Detector:
    _metric_name: MetricName = None
    _threshold: Threshold = None
    _slidingWindow: SlidingWindow = None

    def __init__(self, metric_name: MetricName, threshold: Threshold, sliding_window: SlidingWindow):
        self._metric_name = metric_name
        self._threshold = threshold
        self._slidingWindow = sliding_window
        self._threshold.attach_observer(self._slidingWindow)
        self._count = 0

    def get_metric_name(self):
        return self._metric_name

    def is_slow_io_event(self, io_data_dict_with_disk_name: dict):
        self._count += 1
        if self._count % 15 == 0:
            self._count = 0
            logging.info(f"({self._metric_name}) 's latest threshold is: {self._threshold.get_threshold()}.")
        logging.debug(f'enter Detector: {self}')
        metric_value = get_metric_value_from_io_data_dict_by_metric_name(io_data_dict_with_disk_name, self._metric_name)
        if metric_value is None:
            logging.debug('not found metric value, so return None.')
            return False, None, None
        logging.debug(f'input metric value: {str(metric_value)}')
        self._threshold.push_latest_data_to_queue(metric_value)
        detection_result = self._slidingWindow.is_slow_io_event(metric_value)
        logging.debug(f'Detection result: {str(detection_result)}')
        logging.debug(f'exit Detector: {self}')
        return detection_result

    def __repr__(self):
        return (f'disk_name: {self._metric_name.get_disk_name()}, stage_name: {self._metric_name.get_stage_name()},'
                f' access_type_name: {self._metric_name.get_io_access_type_name()},'
                f' metric_name: {self._metric_name.get_metric_name()}, threshold_type: {self._threshold},'
                f' sliding_window_type: {self._slidingWindow}')
