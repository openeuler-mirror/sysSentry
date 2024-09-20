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

from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional


@dataclass
class BaseData:
    latency: Optional[float] = field(default_factory=lambda: None)
    io_dump: Optional[int] = field(default_factory=lambda: None)
    io_length: Optional[int] = field(default_factory=lambda: None)
    iops: Optional[int] = field(default_factory=lambda: None)


@dataclass
class IOStageData:
    read: BaseData = field(default_factory=lambda: BaseData())
    write: BaseData = field(default_factory=lambda: BaseData())
    flush: BaseData = field(default_factory=lambda: BaseData())
    discard: BaseData = field(default_factory=lambda: BaseData())


@dataclass
class IOData:
    throtl: IOStageData = field(default_factory=lambda: IOStageData())
    wbt: IOStageData = field(default_factory=lambda: IOStageData())
    gettag: IOStageData = field(default_factory=lambda: IOStageData())
    iocost: IOStageData = field(default_factory=lambda: IOStageData())
    plug: IOStageData = field(default_factory=lambda: IOStageData())
    bfq: IOStageData = field(default_factory=lambda: IOStageData())
    hctx: IOStageData = field(default_factory=lambda: IOStageData())
    requeue: IOStageData = field(default_factory=lambda: IOStageData())
    rq_driver: IOStageData = field(default_factory=lambda: IOStageData())
    bio: IOStageData = field(default_factory=lambda: IOStageData())
    time_stamp: float = field(default_factory=lambda: datetime.now().timestamp())


class MetricName:
    _disk_name: str = None
    _stage_name: str = None
    _io_access_type_name: str = None
    _metric_name: str = None

    def __init__(self, disk_name: str, stage_name: str, io_access_type_name: str, metric_name: str):
        self._disk_name = disk_name
        self._stage_name = stage_name
        self._io_access_type_name = io_access_type_name
        self._metric_name = metric_name

    def get_disk_name(self):
        return self._disk_name

    def get_stage_name(self):
        return self._stage_name

    def get_io_access_type_name(self):
        return self._io_access_type_name

    def get_metric_name(self):
        return self._metric_name

    def __repr__(self):
        return (f'disk: {self._disk_name}, stage: {self._stage_name}, io_access_type: {self._io_access_type_name},'
                f'metric: {self._metric_name}')
