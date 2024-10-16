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
from datetime import datetime

from .io_data import MetricName
from .threshold import Threshold
from .sliding_window import SlidingWindow
from .utils import get_metric_value_from_io_data_dict_by_metric_name


class Detector:

    def __init__(self, metric_name: MetricName, threshold: Threshold, sliding_window: SlidingWindow):
        self._metric_name = metric_name
        self._threshold = threshold
        # for when threshold update, it can print latest threshold with metric name
        self._threshold.set_metric_name(self._metric_name)
        self._slidingWindow = sliding_window
        self._threshold.attach_observer(self._slidingWindow)
        self._count = None

    def get_metric_name(self):
        return self._metric_name

    def is_slow_io_event(self, io_data_dict_with_disk_name: dict):
        if self._count is None:
            self._count = datetime.now()
        else:
            now_time = datetime.now()
            time_diff = (now_time - self._count).total_seconds()
            if time_diff >= 60:
                logging.info(f"({self._metric_name}) 's latest threshold is: {self._threshold.get_threshold()}.")
                self._count = None

        logging.debug(f'enter Detector: {self}')
        metric_value = get_metric_value_from_io_data_dict_by_metric_name(io_data_dict_with_disk_name, self._metric_name)
        if metric_value is None:
            logging.debug('not found metric value, so return None.')
            return (False, False), None, None, None
        logging.debug(f'input metric value: {str(metric_value)}')
        self._threshold.push_latest_data_to_queue(metric_value)
        detection_result = self._slidingWindow.is_slow_io_event(metric_value)
        # 检测到慢周期，由Detector负责打印info级别日志
        if detection_result[0][1]:
            logging.info(f'[abnormal period happen]: disk info: {self._metric_name}, window: {detection_result[1]}, '
                         f'current value: {metric_value}, ai threshold: {detection_result[2]}, '
                         f'absolute threshold: {detection_result[3]}')
        else:
            logging.debug(f'Detection result: {str(detection_result)}')
        logging.debug(f'exit Detector: {self}')
        return detection_result

    def __repr__(self):
        return (f'disk_name: {self._metric_name.disk_name}, stage_name: {self._metric_name.stage_name},'
                f' io_type_name: {self._metric_name.io_access_type_name},'
                f' metric_name: {self._metric_name.metric_name}, threshold_type: {self._threshold},'
                f' sliding_window_type: {self._slidingWindow}')


class DiskDetector:

    def __init__(self, disk_name: str):
        self._disk_name = disk_name
        self._detector_list = []

    def add_detector(self, detector: Detector):
        self._detector_list.append(detector)

    def is_slow_io_event(self, io_data_dict_with_disk_name: dict):
        """
        根因诊断逻辑：只有bio阶段发生异常，才认为发生了慢IO事件，即bio阶段异常是慢IO事件的必要条件
        情况一：bio异常，rq_driver也异常，则慢盘
        情况二：bio异常，rq_driver无异常，且有内核IO栈任意阶段异常，则IO栈异常
        情况三：bio异常，rq_driver无异常，且无内核IO栈任意阶段异常，则IO压力大
        情况四：bio异常，则UNKNOWN
        """
        diagnosis_info = {"bio": [], "rq_driver": [], "io_stage": []}
        for detector in self._detector_list:
            # result返回内容：(是否检测到慢IO，是否检测到慢周期)、窗口、ai阈值、绝对阈值
            # 示例： (False, False), self._io_data_queue, self._ai_threshold, self._abs_threshold
            result = detector.is_slow_io_event(io_data_dict_with_disk_name)
            if result[0][0]:
                if detector.get_metric_name().stage_name == "bio":
                    diagnosis_info["bio"].append((detector.get_metric_name(), result))
                elif detector.get_metric_name().stage_name == "rq_driver":
                    diagnosis_info["rq_driver"].append((detector.get_metric_name(), result))
                else:
                    diagnosis_info["io_stage"].append((detector.get_metric_name(), result))

        # 返回内容：（1）是否检测到慢IO事件、（2）MetricName、（3）滑动窗口及阈值、（4）慢IO事件根因
        root_cause = None
        if len(diagnosis_info["bio"]) == 0:
            return False, None, None, None
        elif len(diagnosis_info["rq_driver"]) != 0:
            root_cause = "[Root Cause: disk slow]"
        elif len(diagnosis_info["io_stage"]) != 0:
            stage_list = []
            for io_stage in diagnosis_info["io_stage"]:
                stage_list.append(io_stage[0].stage_name)
            root_cause = f"[Root Cause: io stage slow, stage: {stage_list}]"
        if root_cause is None:
            root_cause = "[Root Cause: high io pressure]"
        return True, diagnosis_info["bio"][0][0], diagnosis_info["bio"][0][1], root_cause

    def __repr__(self):
        msg = f'disk: {self._disk_name}, '
        for detector in self._detector_list:
            msg += f'\n detector: [{detector}]'
        return msg
