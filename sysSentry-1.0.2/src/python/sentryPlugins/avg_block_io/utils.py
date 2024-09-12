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
AVG_VALUE = 0
AVG_COUNT = 1


def get_nested_value(data, keys):
    """get data from nested dict"""
    for key in keys:
        if key in data:
            data = data[key]
        else:
            return None
    return data


def set_nested_value(data, keys, value):
    """set data to nested dict"""
    for key in keys[:-1]:
        if key in data:
            data = data[key]
        else:
            return False
    data[keys[-1]] = value
    return True


def is_abnormal(io_key, io_data):
    """check if latency and iodump win abnormal"""
    for key in ['latency', 'iodump']:
        all_keys = get_nested_value(io_data, io_key)
        if all_keys and key in all_keys:
            win = get_nested_value(io_data, io_key + (key,))
            if win and win.is_abnormal_window():
                return True
    return False


def update_io_avg(old_avg, period_value, win_size):
    """update average of latency window"""
    if old_avg[AVG_COUNT] < win_size:
        new_avg_count = old_avg[AVG_COUNT] + 1
        new_avg_value = (old_avg[AVG_VALUE] * old_avg[AVG_COUNT] + period_value[0]) / new_avg_count
    else:
        new_avg_count = old_avg[AVG_COUNT]
        new_avg_value = (old_avg[AVG_VALUE] * (old_avg[AVG_COUNT] - 1) + period_value[0]) / new_avg_count
    return [new_avg_value, new_avg_count]


def update_io_data(old_avg, period_value, win_size, io_data, io_key):
    """update data of latency and iodump window"""
    all_wins = get_nested_value(io_data, io_key)
    if all_wins and "latency" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["latency"].append_new_period(period_value[0], old_avg[AVG_VALUE])
    if all_wins and "iodump" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["iodump"].append_new_period(period_value[1])


def update_avg_and_check_abnormal(data, io_key, win_size, io_avg_value, io_data):
    """update avg and check abonrmal, return true if win_size full"""
    period_value = get_nested_value(data, io_key)
    old_avg = get_nested_value(io_avg_value, io_key)

    # 更新avg数据
    if old_avg[AVG_COUNT] < win_size:
        set_nested_value(io_avg_value, io_key, update_io_avg(old_avg, period_value, win_size))
        return False

    # 更新win数据 -- 判断异常周期
    update_io_data(old_avg, period_value, win_size, io_data, io_key)
    all_wins = get_nested_value(io_data, io_key)
    if all_wins and 'latency' not in all_wins:
        return True
    period = get_nested_value(io_data, io_key + ("latency",))
    if period and period.is_abnormal_period(period_value[0], old_avg[AVG_VALUE]):
        return True
    set_nested_value(io_avg_value, io_key, update_io_avg(old_avg, period_value, win_size))
    return True
