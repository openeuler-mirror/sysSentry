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
import configparser
import logging
import os

AVG_VALUE = 0
AVG_COUNT = 1

CONF_LOG = 'log'
CONF_LOG_LEVEL = 'level'
LogLevel = {
    "debug": logging.DEBUG,
    "info": logging.INFO,
    "warning": logging.WARNING,
    "error": logging.ERROR,
    "critical": logging.CRITICAL
}


DEFAULT_PARAM = {
    'latency_nvme_ssd': {
        'read_avg_lim': 300,
        'write_avg_lim': 300,
        'read_avg_time': 3,
        'write_avg_time': 3,
        'read_tot_lim': 500,
        'write_tot_lim': 500,
    }, 'latency_sata_ssd' : {
        'read_avg_lim': 10000,
        'write_avg_lim': 10000,
        'read_avg_time': 3,
        'write_avg_time': 3,
        'read_tot_lim': 50000,
        'write_tot_lim': 50000,
    }, 'latency_sata_hdd' : {
        'read_avg_lim': 15000,
        'write_avg_lim': 15000,
        'read_avg_time': 3,
        'write_avg_time': 3,
        'read_tot_lim': 50000,
        'write_tot_lim': 50000
    }, 'iodump': {
        'read_iodump_lim': 0,
        'write_iodump_lim': 0
    }
}


def get_section_value(section_name, config):
    common_param = {}
    config_sec = config[section_name]
    for config_key in DEFAULT_PARAM[section_name]:
        if config_key in config_sec:
            if not config_sec[config_key].isdecimal():
                report_alarm_fail(f"Invalid {section_name}.{config_key} config.")
            common_param[config_key] = int(config_sec[config_key])
        else:
            logging.warning(f"Unset  {section_name}.{config_key} in config file, use {DEFAULT_PARAM[section_name][config_key]} as default")
            common_param[config_key] = DEFAULT_PARAM[section_name][config_key]
    return common_param


def get_log_level(filename):
    if not os.path.exists(filename):
        return logging.INFO

    try:
        config = configparser.ConfigParser()
        config.read(filename)
        if not config.has_option(CONF_LOG, CONF_LOG_LEVEL):
            return logging.INFO
        log_level = config.get(CONF_LOG, CONF_LOG_LEVEL)

        if log_level.lower() in LogLevel:
            return LogLevel.get(log_level.lower())
        return logging.INFO
    except configparser.Error:
        return logging.INFO


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


def get_win_data(disk_name, rw, io_data):
    """get latency and iodump win data"""
    latency = ''
    iodump = ''
    for stage_name in io_data[disk_name]:
        if 'latency' in io_data[disk_name][stage_name][rw]:
            latency_list = io_data[disk_name][stage_name][rw]['latency'].window_data_to_string()
            latency += f'{stage_name}: [{latency_list}], '
        if 'iodump' in io_data[disk_name][stage_name][rw]:
            iodump_list = io_data[disk_name][stage_name][rw]['iodump'].window_data_to_string()
            iodump += f'{stage_name}: [{iodump_list}], '
    return {"latency": latency[:-2], "iodump": iodump[:-2]}


def is_abnormal(io_key, io_data):
    """check if latency and iodump win abnormal"""
    abnormal_list = ''
    for key in ['latency', 'iodump']:
        all_keys = get_nested_value(io_data, io_key)
        if all_keys and key in all_keys:
            win = get_nested_value(io_data, io_key + (key,))
            if win and win.is_abnormal_window():
                abnormal_list += key + ', '
    if not abnormal_list:
        return False, abnormal_list
    return True, abnormal_list[:-2]


def update_io_avg(old_avg, period_value, win_size):
    """update average of latency window"""
    if old_avg[AVG_COUNT] < win_size:
        new_avg_count = old_avg[AVG_COUNT] + 1
        new_avg_value = (old_avg[AVG_VALUE] * old_avg[AVG_COUNT] + period_value[0]) / new_avg_count
    else:
        new_avg_count = old_avg[AVG_COUNT]
        new_avg_value = (old_avg[AVG_VALUE] * (old_avg[AVG_COUNT] - 1) + period_value[0]) / new_avg_count
    return [new_avg_value, new_avg_count]


def update_io_period(old_avg, period_value, io_data, io_key):
    """update period of latency and iodump window"""
    all_wins = get_nested_value(io_data, io_key)
    if all_wins and "latency" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["latency"].append_new_period(period_value[0], old_avg[AVG_VALUE])
    if all_wins and "iodump" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["iodump"].append_new_period(period_value[1])


def update_io_data(period_value, io_data, io_key):
    """update data of latency and iodump window"""
    all_wins = get_nested_value(io_data, io_key)
    if all_wins and "latency" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["latency"].append_new_data(period_value[0])
    if all_wins and "iodump" in all_wins:
        io_data[io_key[0]][io_key[1]][io_key[2]]["iodump"].append_new_data(period_value[1])


def log_abnormal_period(old_avg, period_value, io_data, io_key):
    """record log of abnormal period"""
    all_wins = get_nested_value(io_data, io_key)
    if all_wins and "latency" in all_wins:
        if all_wins["latency"].is_abnormal_period(period_value[0], old_avg[AVG_VALUE]):
            logging.info(f"[abnormal_period] disk: {io_key[0]}, stage: {io_key[1]}, iotype: {io_key[2]}, "
                            f"type: latency, avg: {round(old_avg[AVG_VALUE], 3)}, curr_val: {period_value[0]}")
    if all_wins and "iodump" in all_wins:
        if all_wins["iodump"].is_abnormal_period(period_value[1]):
            logging.info(f"[abnormal_period] disk: {io_key[0]}, stage: {io_key[1]}, iotype: {io_key[2]}, "
                            f"type: iodump, curr_val: {period_value[1]}")


def log_slow_win(msg, reason):
    """record log of slow win"""
    logging.warning(f"[SLOW IO] disk: {msg['driver_name']}, stage: {msg['block_stack']}, "
                    f"iotype: {msg['io_type']}, type: {msg['alarm_type']}, reason: {reason}")
    logging.info(f"latency: {msg['details']['latency']}")
    logging.info(f"iodump: {msg['details']['iodump']}")


def update_avg_and_check_abnormal(data, io_key, win_size, io_avg_value, io_data):
    """update avg and check abonrmal, return true if win_size full"""
    period_value = get_nested_value(data, io_key)
    old_avg = get_nested_value(io_avg_value, io_key)

    # 更新avg数据
    update_io_data(period_value, io_data, io_key)
    if old_avg[AVG_COUNT] < win_size:
        set_nested_value(io_avg_value, io_key, update_io_avg(old_avg, period_value, win_size))
        return False

    # 打印异常周期数据
    log_abnormal_period(old_avg, period_value, io_data, io_key)

    # 更新win数据 -- 判断异常周期
    update_io_period(old_avg, period_value, io_data, io_key)
    all_wins = get_nested_value(io_data, io_key)
    if not all_wins or 'latency' not in all_wins:
        return True
    period = get_nested_value(io_data, io_key + ("latency",))
    if period and period.is_abnormal_period(period_value[0], old_avg[AVG_VALUE]):
        return True
    set_nested_value(io_avg_value, io_key, update_io_avg(old_avg, period_value, win_size))
    return True
