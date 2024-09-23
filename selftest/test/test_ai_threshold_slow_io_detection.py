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

import unittest
import numpy as np

from sentryPlugins.ai_threshold_slow_io_detection.threshold import AbsoluteThreshold, BoxplotThreshold, NSigmaThreshold
from sentryPlugins.ai_threshold_slow_io_detection.sliding_window import (NotContinuousSlidingWindow,
                                                                         ContinuousSlidingWindow, MedianSlidingWindow)


def _get_boxplot_threshold(data_list: list, parameter):
    q1 = np.percentile(data_list, 25)
    q3 = np.percentile(data_list, 75)
    iqr = q3 - q1
    return q3 + parameter * iqr


def _get_n_sigma_threshold(data_list: list, parameter):
    mean = np.mean(data_list)
    std = np.std(data_list)
    return mean + parameter * std


class Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        print("UnitTest Begin...")

    @classmethod
    def tearDownClass(cls):
        print("UnitTest End...")

    def setUp(self):
        print("Begin...")

    def tearDown(self):
        print("End...")

    def test_absolute_threshold(self):
        absolute = AbsoluteThreshold()
        self.assertEqual(None, absolute.get_threshold())
        self.assertFalse(absolute.is_abnormal(5000))
        absolute.set_threshold(40)
        self.assertEqual(40, absolute.get_threshold())
        self.assertTrue(absolute.is_abnormal(50))

    def test_boxplot_threshold(self):
        boxplot = BoxplotThreshold(1.5, 5, 1)
        # 阶段1：尚未初始化
        self.assertEqual(None, boxplot.get_threshold())
        self.assertFalse(boxplot.is_abnormal(5000))
        # 往boxplot中插入5个元素后，会生成阈值
        data_list = [20, 20, 20, 30, 10]
        for data in data_list:
            boxplot.push_latest_data_to_queue(data)
        # 阶段2：初始化
        boxplot_threshold = boxplot.get_threshold()
        self.assertEqual(_get_boxplot_threshold(data_list, 1.5), boxplot_threshold)
        self.assertTrue(boxplot.is_abnormal(5000))
        data_list.pop(0)
        data_list.append(100)
        boxplot.push_latest_data_to_queue(100)
        # 阶段3：更新阈值
        boxplot_threshold = boxplot.get_threshold()
        self.assertEqual(_get_boxplot_threshold(data_list, 1.5), boxplot_threshold)

    def test_n_sigma_threshold(self):
        n_sigma = NSigmaThreshold(3, 5, 1)
        self.assertEqual(None, n_sigma.get_threshold())
        self.assertFalse(n_sigma.is_abnormal(5000))
        data_list = [20, 20, 20, 30, 10]
        for data in data_list:
            n_sigma.push_latest_data_to_queue(data)
        n_sigma_threshold = n_sigma.get_threshold()
        self.assertEqual(_get_n_sigma_threshold(data_list, 3), n_sigma_threshold)
        self.assertTrue(n_sigma.is_abnormal(5000))
        data_list.pop(0)
        data_list.append(100)
        n_sigma.push_latest_data_to_queue(100)
        # 阶段3：更新阈值
        n_sigma_threshold = n_sigma.get_threshold()
        self.assertEqual(_get_n_sigma_threshold(data_list, 3), n_sigma_threshold)

    def test_not_continuous_sliding_window(self):
        not_continuous = NotContinuousSlidingWindow(5, 3)
        boxplot_threshold = BoxplotThreshold(1.5, 10, 8)
        boxplot_threshold.attach_observer(not_continuous)
        data_list1 = [19, 20, 20, 20, 20, 20, 22, 24, 23, 20]
        for data in data_list1:
            boxplot_threshold.push_latest_data_to_queue(data)
            result = not_continuous.is_slow_io_event(data)
            self.assertFalse(result[0])
        self.assertEqual(23.75, boxplot_threshold.get_threshold())
        boxplot_threshold.push_latest_data_to_queue(24)
        result = not_continuous.is_slow_io_event(24)
        self.assertFalse(result[0])
        boxplot_threshold.push_latest_data_to_queue(25)
        result = not_continuous.is_slow_io_event(25)
        self.assertTrue(result[0])
        data_list2 = [20, 20, 20, 20, 20, 20]
        for data in data_list2:
            boxplot_threshold.push_latest_data_to_queue(data)
            result = not_continuous.is_slow_io_event(data)
            self.assertFalse(result[0])
        self.assertEqual(25.625, boxplot_threshold.get_threshold())

    def test_continuous_sliding_window(self):
        continuous = ContinuousSlidingWindow(5, 3)
        boxplot_threshold = BoxplotThreshold(1.5, 10, 8)
        boxplot_threshold.attach_observer(continuous)
        data_list = [19, 20, 20, 20, 20, 20, 22, 24, 23, 20]
        for data in data_list:
            boxplot_threshold.push_latest_data_to_queue(data)
            result = continuous.is_slow_io_event(data)
            self.assertFalse(result[0])
        self.assertEqual(23.75, boxplot_threshold.get_threshold())
        # 没有三个异常点
        self.assertFalse(continuous.is_slow_io_event(25)[0])
        # 不连续的三个异常点
        self.assertFalse(continuous.is_slow_io_event(25)[0])
        # 连续的三个异常点
        self.assertTrue(continuous.is_slow_io_event(25)[0])

    def test_median_sliding_window(self):
        median = MedianSlidingWindow(5, 3)
        absolute_threshold = AbsoluteThreshold(10, 8)
        absolute_threshold.attach_observer(median)
        absolute_threshold.set_threshold(24.5)
        data_list = [24, 24, 24, 25, 25]
        for data in data_list:
            self.assertFalse(median.is_slow_io_event(data)[0])
        self.assertTrue(median.is_slow_io_event(25)[0])

    def test_parse_collect_data(self):
        collect = {
            "read": [1.0, 2.0, 3.0, 4.0],
            "write": [5.0, 6.0, 7.0, 8.0],
            "flush": [9.0, 10.0, 11.0, 12.0],
            "discard": [13.0, 14.0, 15.0, 16.0],
        }
        from io_data import BaseData
        from data_access import _get_io_stage_data

        io_data = _get_io_stage_data(collect)
        self.assertEqual(
            io_data.read, BaseData(latency=1.0, io_dump=2.0, io_length=3.0, iops=4.0)
        )
        self.assertEqual(
            io_data.write, BaseData(latency=5.0, io_dump=6.0, io_length=7.0, iops=8.0)
        )
        self.assertEqual(
            io_data.flush, BaseData(latency=9.0, io_dump=10.0, io_length=11.0, iops=12.0)
        )
        self.assertEqual(
            io_data.discard, BaseData(latency=13.0, io_dump=14.0, io_length=15.0, iops=16.0)
        )
