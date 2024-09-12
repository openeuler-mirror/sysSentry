# coding: utf-8
# Copyright (c) 2023 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

"""
project setup.
"""
from setuptools import setup, find_packages


setup(
    name="syssentry",
    version="1.0.2",
    description="System inspection framework tool set",
    packages=find_packages(),
    include_package_data=True,
    classifiers=[
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
    ],
    entry_points={
        'console_scripts': [
            'cpu_sentry=syssentry.cpu_sentry:main',
            'syssentry=syssentry.syssentry:main',
            'xalarmd=xalarm.xalarm_daemon:alarm_process_create',
            'sentryCollector=sentryCollector.collectd:main',
            'avg_block_io=sentryPlugins.avg_block_io.avg_block_io:main'
        ]
    },
)
