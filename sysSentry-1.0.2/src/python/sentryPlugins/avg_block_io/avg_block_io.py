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
import signal
import configparser
import time

from .stage_window import IoWindow, IoDumpWindow
from .module_conn import avg_is_iocollect_valid, avg_get_io_data, report_alarm_fail, process_report_data, sig_handler, get_disk_type_by_name
from .utils import update_avg_and_check_abnormal, get_log_level, get_section_value
from sentryCollector.collect_plugin import Disk_Type

CONFIG_FILE = "/etc/sysSentry/plugins/avg_block_io.ini"

def log_invalid_keys(not_in_list, keys_name, config_list, default_list):
    """print invalid log"""
    if config_list and not_in_list:
        logging.warning("{} in common.{} are not valid, set {}={}".format(not_in_list, keys_name, keys_name, default_list))
    elif config_list == ["default"]:
        logging.warning("Default {} use {}".format(keys_name, default_list))


def read_config_common(config):
    """read config file, get [common] section value"""    
    if not config.has_section("common"):
        report_alarm_fail("Cannot find common section in config file")

    try:
        disk_name = config.get("common", "disk")
        disk = [] if disk_name == "default" else disk_name.split(",")
    except configparser.NoOptionError:
        disk = []
        logging.warning("Unset common.disk, set to default")

    try:
        stage_name = config.get("common", "stage")
        stage = [] if stage_name == "default" else stage_name.split(",")
    except configparser.NoOptionError:
        stage = []
        logging.warning("Unset common.stage, set to read,write")

    if len(disk) > 10:
        logging.warning("Too many common.disks, record only max 10 disks")
        disk = disk[:10]

    try:
        iotype_name = config.get("common", "iotype").split(",")
        iotype_list = [rw.lower() for rw in iotype_name if rw.lower() in ['read', 'write']]
        err_iotype = [rw.lower() for rw in iotype_name if rw.lower() not in ['read', 'write']]

        if err_iotype:
            report_alarm_fail("Invalid common.iotype config")

    except configparser.NoOptionError:
        iotype_list = ["read", "write"]
        logging.warning("Unset common.iotype, set to default")
    
    try:
        period_time = int(config.get("common", "period_time"))
        if not (1 <= period_time <= 300):
            raise ValueError("Invalid period_time")
    except ValueError:
        report_alarm_fail("Invalid common.period_time")
    except configparser.NoOptionError:
        period_time = 1
        logging.warning("Unset common.period_time, use 1s as default")

    return period_time, disk, stage, iotype_list


def read_config_algorithm(config):
    """read config file, get [algorithm] section value"""
    if not config.has_section("algorithm"):
        report_alarm_fail("Cannot find algorithm section in config file")

    try:
        win_size = int(config.get("algorithm", "win_size"))
        if not (1 <= win_size <= 300):
            raise ValueError("Invalid algorithm.win_size")
    except ValueError:
        report_alarm_fail("Invalid algorithm.win_size config")
    except configparser.NoOptionError:
        win_size = 30
        logging.warning("Unset algorithm.win_size, use 30 as default")
    
    try:
        win_threshold = int(config.get("algorithm", "win_threshold"))
        if win_threshold < 1 or win_threshold > 300 or win_threshold > win_size:
            raise ValueError("Invalid algorithm.win_threshold")
    except ValueError:
        report_alarm_fail("Invalid algorithm.win_threshold config")
    except configparser.NoOptionError:
        win_threshold = 6
        logging.warning("Unset algorithm.win_threshold, use 6 as default")

    return win_size, win_threshold


def read_config_latency(config):
    """read config file, get [latency_xxx] section value"""
    common_param = {}
    for type_name in Disk_Type:
        section_name = f"latency_{Disk_Type[type_name]}"
        if not config.has_section(section_name):
            report_alarm_fail(f"Cannot find {section_name} section in config file")

        common_param[Disk_Type[type_name]] = get_section_value(section_name, config)
    return common_param


def read_config_iodump(config):
    """read config file, get [iodump] section value"""
    common_param = {}
    section_name = "iodump"
    if not config.has_section(section_name):
        report_alarm_fail(f"Cannot find {section_name} section in config file")

    return get_section_value(section_name, config) 


def read_config_stage(config, stage, iotype_list, curr_disk_type):
    """read config file, get [STAGE_NAME_diskType] section value"""
    res = {}
    section_name = f"{stage}_{curr_disk_type}"
    if not config.has_section(section_name):
        return res

    for key in config[section_name]:
        if config[stage][key].isdecimal():
            res[key] = int(config[stage][key])

    return res


def init_io_win(io_dic, config, common_param):
    """initialize windows of latency, iodump, and dict of avg_value"""
    iotype_list = io_dic["iotype_list"]
    io_data = {}
    io_avg_value = {}
    for disk_name in io_dic["disk_list"]:
        io_data[disk_name] = {}
        io_avg_value[disk_name] = {}
        curr_disk_type = get_disk_type_by_name(disk_name)
        for stage_name in io_dic["stage_list"]:
            io_data[disk_name][stage_name] = {}
            io_avg_value[disk_name][stage_name] = {}
            # 解析stage配置
            curr_stage_param = read_config_stage(config, stage_name, iotype_list, curr_disk_type)
            for rw in iotype_list:
                io_data[disk_name][stage_name][rw] = {}
                io_avg_value[disk_name][stage_name][rw] = [0, 0]

                # 对每个rw创建latency和iodump窗口
                avg_lim_key = "{}_avg_lim".format(rw)
                avg_time_key = "{}_avg_time".format(rw)
                tot_lim_key = "{}_tot_lim".format(rw)
                iodump_lim_key = "{}_iodump_lim".format(rw)

                # 获取值，优先从 curr_stage_param 获取，如果不存在，则从 common_param 获取
                avg_lim_value = curr_stage_param.get(avg_lim_key, common_param.get(curr_disk_type, {}).get(avg_lim_key))
                avg_time_value = curr_stage_param.get(avg_time_key, common_param.get(curr_disk_type, {}).get(avg_time_key))
                tot_lim_value = curr_stage_param.get(tot_lim_key, common_param.get(curr_disk_type, {}).get(tot_lim_key))
                iodump_lim_value = curr_stage_param.get(iodump_lim_key, common_param.get("iodump", {}).get(iodump_lim_key))

                if avg_lim_value and avg_time_value and tot_lim_value:
                    io_data[disk_name][stage_name][rw]["latency"] = IoWindow(window_size=io_dic["win_size"], window_threshold=io_dic["win_threshold"], abnormal_multiple=avg_time_value, abnormal_multiple_lim=avg_lim_value, abnormal_time=tot_lim_value)
                    logging.debug("Successfully create {}-{}-{}-latency window".format(disk_name, stage_name, rw))

                if iodump_lim_value is not None:
                    io_data[disk_name][stage_name][rw]["iodump"] = IoDumpWindow(window_size=io_dic["win_size"], window_threshold=io_dic["win_threshold"], abnormal_time=iodump_lim_value)
                    logging.debug("Successfully create {}-{}-{}-iodump window".format(disk_name, stage_name, rw))
    return io_data, io_avg_value


def get_valid_disk_stage_list(io_dic, config_disk, config_stage):
    """get disk_list and stage_list by sentryCollector"""
    json_data = avg_is_iocollect_valid(io_dic, config_disk, config_stage)

    all_disk_set = json_data.keys()
    all_stage_set = set()
    for disk_stage_list in json_data.values():
        all_stage_set.update(disk_stage_list)

    disk_list = [key for key in all_disk_set if key in config_disk]
    not_in_disk_list = [key for key in config_disk if key not in all_disk_set]

    stage_list = [key for key in all_stage_set if key in config_stage]
    not_in_stage_list = [key for key in config_stage if key not in all_stage_set]

    if not_in_stage_list:
        report_alarm_fail(f"Invalid common.stage_list config, cannot set {not_in_stage_list}")

    if not config_disk and not not_in_disk_list:
        disk_list = [key for key in all_disk_set]

    if not config_stage and not not_in_stage_list:
        stage_list = [key for key in all_stage_set]

    disk_list = disk_list[:10] if len(disk_list) > 10 else disk_list

    if not stage_list or not disk_list:
        report_alarm_fail("Cannot get valid disk name or stage name.")

    log_invalid_keys(not_in_disk_list, 'disk', config_disk, disk_list)

    return disk_list, stage_list


def main_loop(io_dic, io_data, io_avg_value):
    """main loop of avg_block_io"""
    period_time = io_dic["period_time"]
    disk_list = io_dic["disk_list"]
    stage_list = io_dic["stage_list"]
    iotype_list = io_dic["iotype_list"]
    win_size = io_dic["win_size"]
    # 开始循环
    while True:
        # 等待x秒
        time.sleep(period_time)

        # 采集模块对接，获取周期数据
        curr_period_data = avg_get_io_data(io_dic)

        # 处理周期数据
        reach_size = False
        for disk_name in disk_list:
            for stage_name in stage_list:
                for rw in iotype_list:
                    if disk_name in curr_period_data and stage_name in curr_period_data[disk_name] and rw in curr_period_data[disk_name][stage_name]:
                        io_key = (disk_name, stage_name, rw)
                        reach_size = update_avg_and_check_abnormal(curr_period_data, io_key, win_size, io_avg_value, io_data)

        # win_size不满时不进行告警判断
        if not reach_size:
            continue

        # 判断异常窗口、异常场景
        for disk_name in disk_list:
            for rw in iotype_list:
                process_report_data(disk_name, rw, io_data)


def main():
    """main func"""
    # 注册停止信号-2/-15
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    log_level = get_log_level(CONFIG_FILE)
    log_format = "%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s"

    logging.basicConfig(level=log_level, format=log_format)

    # 初始化配置读取
    config = configparser.ConfigParser(comment_prefixes=('#', ';'))
    try:
        config.read(CONFIG_FILE)
    except configparser.Error:
        report_alarm_fail("Failed to read config file")

    io_dic = {}

    # 读取配置文件 -- common段
    io_dic["period_time"], disk, stage, io_dic["iotype_list"] = read_config_common(config)

    # 采集模块对接，is_iocollect_valid()
    io_dic["disk_list"], io_dic["stage_list"] = get_valid_disk_stage_list(io_dic, disk, stage)

    if "bio" not in io_dic["stage_list"]:
        report_alarm_fail("Cannot run avg_block_io without bio stage")

    # 初始化窗口 -- config读取，对应is_iocollect_valid返回的结果
    # step1. 解析公共配置 --- algorithm
    io_dic["win_size"], io_dic["win_threshold"] = read_config_algorithm(config)

    # step2. 解析公共配置 --- latency_xxx
    common_param = read_config_latency(config)

    # step3. 解析公共配置 --- iodump
    common_param['iodump'] = read_config_iodump(config)

    # step4. 循环创建窗口
    io_data, io_avg_value = init_io_win(io_dic, config, common_param)

    main_loop(io_dic, io_data, io_avg_value)
