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
from enum import Enum
import queue
import numpy as np
import math

from .sliding_window import SlidingWindow


class ThresholdState(Enum):
    INIT = 0
    START = 1


class Threshold:

    def __init__(self, data_queue_size: int = 10000, data_queue_update_size: int = 1000):
        self._observer = None
        self.data_queue = queue.Queue(data_queue_size)
        self.data_queue_update_size = data_queue_update_size
        self.new_data_size = 0
        self.threshold_state = ThresholdState.INIT
        self.threshold = math.inf
        self.metric_name = None

    def set_threshold(self, threshold):
        self.threshold = threshold
        self.threshold_state = ThresholdState.START
        self.notify_observer()

    def set_metric_name(self, metric_name):
        self.metric_name = metric_name

    def get_threshold(self):
        if self.threshold_state == ThresholdState.INIT:
            return None
        return self.threshold

    def is_abnormal(self, data):
        if self.threshold_state == ThresholdState.INIT:
            return False
        return data >= self.threshold

    # 使用观察者模式，当阈值更新时，自动同步刷新滑窗中的阈值
    def attach_observer(self, observer: SlidingWindow):
        self._observer = observer

    def notify_observer(self):
        if self._observer is not None:
            self._observer.update(self.threshold)

    def push_latest_data_to_queue(self, data):
        pass

    def __repr__(self):
        return "Threshold"

    def __str__(self):
        return "Threshold"


class AbsoluteThreshold(Threshold):
    def __init__(self, data_queue_size: int = 10000, data_queue_update_size: int = 1000, **kwargs):
        super().__init__(data_queue_size, data_queue_update_size)

    def push_latest_data_to_queue(self, data):
        pass

    def __repr__(self):
        return "[AbsoluteThreshold]"

    def __str__(self):
        return "absolute"


class BoxplotThreshold(Threshold):
    def __init__(self, boxplot_parameter: float = 1.5, data_queue_size: int = 10000, data_queue_update_size: int = 1000, **kwargs):
        super().__init__(data_queue_size, data_queue_update_size)
        self.parameter = boxplot_parameter

    def _update_threshold(self):
        old_threshold = self.threshold
        data = list(self.data_queue.queue)
        q1 = np.percentile(data, 25)
        q3 = np.percentile(data, 75)
        iqr = q3 - q1
        self.threshold = q3 + self.parameter * iqr
        if self.threshold_state == ThresholdState.INIT:
            self.threshold_state = ThresholdState.START
        logging.info(f"MetricName: [{self.metric_name}]'s threshold update, old is: {old_threshold} -> new is: {self.threshold}")
        self.notify_observer()

    def push_latest_data_to_queue(self, data):
        if data < 1e-6:
            return
        try:
            self.data_queue.put(data, block=False)
        except queue.Full:
            self.data_queue.get()
            self.data_queue.put(data)
        self.new_data_size += 1
        if (self.data_queue.full() and (self.threshold_state == ThresholdState.INIT or
                                        (self.threshold_state == ThresholdState.START and
                                         self.new_data_size >= self.data_queue_update_size))):
            self._update_threshold()
            self.new_data_size = 0

    def __repr__(self):
        return f"[BoxplotThreshold, param is: {self.parameter}, train_size: {self.data_queue.maxsize}, update_size: {self.data_queue_update_size}]"

    def __str__(self):
        return "boxplot"


class NSigmaThreshold(Threshold):
    def __init__(self, n_sigma_parameter: float = 3.0, data_queue_size: int = 10000, data_queue_update_size: int = 1000, **kwargs):
        super().__init__(data_queue_size, data_queue_update_size)
        self.parameter = n_sigma_parameter

    def _update_threshold(self):
        old_threshold = self.threshold
        data = list(self.data_queue.queue)
        mean = np.mean(data)
        std = np.std(data)
        self.threshold = mean + self.parameter * std
        if self.threshold_state == ThresholdState.INIT:
            self.threshold_state = ThresholdState.START
        logging.info(f"MetricName: [{self.metric_name}]'s threshold update, old is: {old_threshold} -> new is: {self.threshold}")
        self.notify_observer()

    def push_latest_data_to_queue(self, data):
        if data < 1e-6:
            return
        try:
            self.data_queue.put(data, block=False)
        except queue.Full:
            self.data_queue.get()
            self.data_queue.put(data)
        self.new_data_size += 1
        if (self.data_queue.full() and (self.threshold_state == ThresholdState.INIT or
                                        (self.threshold_state == ThresholdState.START and
                                         self.new_data_size >= self.data_queue_update_size))):
            self._update_threshold()
            self.new_data_size = 0

    def __repr__(self):
        return f"[NSigmaThreshold, param is: {self.parameter}, train_size: {self.data_queue.maxsize}, update_size: {self.data_queue_update_size}]"

    def __str__(self):
        return "n_sigma"


class ThresholdType(Enum):
    AbsoluteThreshold = 0
    BoxplotThreshold = 1
    NSigmaThreshold = 2


class ThresholdFactory:
    def get_threshold(self, threshold_type: ThresholdType, *args, **kwargs):
        if threshold_type == ThresholdType.AbsoluteThreshold:
            return AbsoluteThreshold(*args, **kwargs)
        elif threshold_type == ThresholdType.BoxplotThreshold:
            return BoxplotThreshold(*args, **kwargs)
        elif threshold_type == ThresholdType.NSigmaThreshold:
            return NSigmaThreshold(*args, **kwargs)
        else:
            raise ValueError(f"Invalid threshold type: {threshold_type}")
