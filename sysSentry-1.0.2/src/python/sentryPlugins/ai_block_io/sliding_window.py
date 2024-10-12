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

from enum import Enum, unique
import numpy as np


@unique
class SlidingWindowType(Enum):
    NotContinuousSlidingWindow = 0
    ContinuousSlidingWindow = 1
    MedianSlidingWindow = 2


class SlidingWindow:
    def __init__(self, queue_length: int, threshold: int, abs_threshold: int = None):
        self._queue_length = queue_length
        self._queue_threshold = threshold
        self._ai_threshold = None
        self._abs_threshold = abs_threshold
        self._io_data_queue = []
        self._io_data_queue_abnormal_tag = []

    def push(self, data: float):
        if len(self._io_data_queue) == self._queue_length:
            self._io_data_queue.pop(0)
            self._io_data_queue_abnormal_tag.pop(0)
        self._io_data_queue.append(data)
        self._io_data_queue_abnormal_tag.append(
            (
                data >= self._ai_threshold
                or self._abs_threshold is not None
                and data >= self._abs_threshold
            )
            if self._ai_threshold is not None
            else False
        )

    def update(self, threshold):
        if self._ai_threshold == threshold:
            return
        self._ai_threshold = threshold
        self._io_data_queue_abnormal_tag.clear()
        for data in self._io_data_queue:
            self._io_data_queue_abnormal_tag.append(data >= self._ai_threshold)

    def is_slow_io_event(self, data):
        return False, None, None, None

    def __repr__(self):
        return "[SlidingWindow]"


class NotContinuousSlidingWindow(SlidingWindow):
    def is_slow_io_event(self, data):
        super().push(data)
        if len(self._io_data_queue) < self._queue_length or self._ai_threshold is None:
            return False, self._io_data_queue, self._ai_threshold, self._abs_threshold
        if self._io_data_queue_abnormal_tag.count(True) >= self._queue_threshold:
            return True, self._io_data_queue, self._ai_threshold, self._abs_threshold
        return False, self._io_data_queue, self._ai_threshold, self._abs_threshold

    def __repr__(self):
        return f"[NotContinuousSlidingWindow, window size: {self._queue_length}, threshold: {self._queue_threshold}]"


class ContinuousSlidingWindow(SlidingWindow):
    def is_slow_io_event(self, data):
        super().push(data)
        if len(self._io_data_queue) < self._queue_length or self._ai_threshold is None:
            return False, self._io_data_queue, self._ai_threshold, self._abs_threshold
        consecutive_count = 0
        for tag in self._io_data_queue_abnormal_tag:
            if tag:
                consecutive_count += 1
                if consecutive_count >= self._queue_threshold:
                    return (
                        True,
                        self._io_data_queue,
                        self._ai_threshold,
                        self._abs_threshold,
                    )
            else:
                consecutive_count = 0
        return False, self._io_data_queue, self._ai_threshold, self._abs_threshold

    def __repr__(self):
        return f"[ContinuousSlidingWindow, window size: {self._queue_length}, threshold: {self._queue_threshold}]"


class MedianSlidingWindow(SlidingWindow):
    def is_slow_io_event(self, data):
        super().push(data)
        if len(self._io_data_queue) < self._queue_length or self._ai_threshold is None:
            return False, self._io_data_queue, self._ai_threshold, self._abs_threshold
        median = np.median(self._io_data_queue)
        if median >= self._ai_threshold:
            return True, self._io_data_queue, self._ai_threshold, self._abs_threshold
        return False, self._io_data_queue, self._ai_threshold, self._abs_threshold

    def __repr__(self):
        return f"[MedianSlidingWindow, window size: {self._queue_length}]"


class SlidingWindowFactory:
    def get_sliding_window(
        self, sliding_window_type: SlidingWindowType, *args, **kwargs
    ):
        if sliding_window_type == SlidingWindowType.NotContinuousSlidingWindow:
            return NotContinuousSlidingWindow(*args, **kwargs)
        elif sliding_window_type == SlidingWindowType.ContinuousSlidingWindow:
            return ContinuousSlidingWindow(*args, **kwargs)
        elif sliding_window_type == SlidingWindowType.MedianSlidingWindow:
            return MedianSlidingWindow(*args, **kwargs)
        else:
            return NotContinuousSlidingWindow(*args, **kwargs)
