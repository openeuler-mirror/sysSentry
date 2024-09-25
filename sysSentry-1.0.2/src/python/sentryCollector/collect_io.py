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

"""
collect module
"""
import os
import time
import logging
import threading

from .collect_config import CollectConfig

Io_Category = ["read", "write", "flush", "discard"]
IO_GLOBAL_DATA = {}
IO_CONFIG_DATA = []

class IoStatus():
    TOTAL = 0
    FINISH = 1
    LATENCY = 2

class CollectIo():

    def __init__(self, module_config):

        io_config = module_config.get_io_config()

        self.period_time = io_config['period_time']
        self.max_save = io_config['max_save']
        disk_str = io_config['disk']

        self.disk_map_stage = {}
        self.window_value = {}

        self.loop_all = False

        if disk_str == "default":
            self.loop_all = True
        else:
            self.disk_list = disk_str.strip().split(',')

        self.stop_event = threading.Event()

        IO_CONFIG_DATA.append(self.period_time)
        IO_CONFIG_DATA.append(self.max_save)

    def get_blk_io_hierarchy(self, disk_name, stage_list):
        stats_file = '/sys/kernel/debug/block/{}/blk_io_hierarchy/stats'.format(disk_name)
        try:
            with open(stats_file, 'r') as file:
                lines = file.read()
        except FileNotFoundError:
            logging.error("The file %s does not exist", stats_file)
            return -1
        except Exception as e:
            logging.error("An error occurred3: %s", e)
            return -1

        curr_value = lines.strip().split('\n')

        for stage_val in curr_value:
            stage = stage_val.split(' ')[0]
            if (len(self.window_value[disk_name][stage])) >= 2:
                self.window_value[disk_name][stage].pop(0)

            curr_stage_value = stage_val.split(' ')[1:-1]
            self.window_value[disk_name][stage].append(curr_stage_value)
        return 0

    def append_period_lat(self, disk_name, stage_list):
        for stage in stage_list:
            if len(self.window_value[disk_name][stage]) < 2:
                return
            curr_stage_value = self.window_value[disk_name][stage][-1]
            last_stage_value = self.window_value[disk_name][stage][-2]

            for index in range(len(Io_Category)):
                # read=0, write=1, flush=2, discard=3
                if (len(IO_GLOBAL_DATA[disk_name][stage][Io_Category[index]])) >= self.max_save:
                    IO_GLOBAL_DATA[disk_name][stage][Io_Category[index]].pop()

                curr_lat = self.get_latency_value(curr_stage_value, last_stage_value, index)
                curr_iops = self.get_iops(curr_stage_value, last_stage_value, index)
                curr_io_length = self.get_io_length(curr_stage_value, last_stage_value, index)
                curr_io_dump = self.get_io_dump(disk_name, stage, index)

                IO_GLOBAL_DATA[disk_name][stage][Io_Category[index]].insert(0, [curr_lat, curr_io_dump, curr_io_length, curr_iops])

    def get_iops(self, curr_stage_value, last_stage_value, category):
        try:
            finish = int(curr_stage_value[category * 3 + IoStatus.FINISH]) - int(last_stage_value[category * 3 + IoStatus.FINISH])
        except ValueError as e:
            logging.error("get_iops convert to int failed, %s", e)
            return 0
        value = finish / self.period_time
        if value.is_integer():
            return int(value)
        else:
            return round(value, 1)

    def get_latency_value(self, curr_stage_value, last_stage_value, category):
        try:
            finish = int(curr_stage_value[category * 3 + IoStatus.FINISH]) - int(last_stage_value[category * 3 + IoStatus.FINISH])
            lat_time = (int(curr_stage_value[category * 3 + IoStatus.LATENCY]) - int(last_stage_value[category * 3 + IoStatus.LATENCY]))
        except ValueError as e:
            logging.error("get_latency_value convert to int failed, %s", e)
            return 0
        if finish <= 0 or lat_time <= 0:
            return 0
        value = lat_time / finish / 1000 / 1000
        if value.is_integer():
            return int(value)
        else:
            return round(value, 1)

    def get_io_length(self, curr_stage_value, last_stage_value, category):
        try:
            finish = int(curr_stage_value[category * 3 + IoStatus.FINISH]) - int(last_stage_value[category * 3 + IoStatus.FINISH])
        except ValueError as e:
            logging.error("get_io_length convert to int failed, %s", e)
            return 0
        value = finish / self.period_time / 1000 / 1000
        if value.is_integer():
            return int(value)
        else:
            return round(value, 1)

    def get_io_dump(self, disk_name, stage, category):
        io_dump_file = '/sys/kernel/debug/block/{}/blk_io_hierarchy/{}/io_dump'.format(disk_name, stage)
        count = 0
        try:
            with open(io_dump_file, 'r') as file:
                for line in file:
                    count += line.count('.op=' + Io_Category[category])
        except FileNotFoundError:
            logging.error("The file %s does not exist.", io_dump_file)
            return count
        except Exception as e:
            logging.error("An error occurred1: %s", e)
            return count
        return count

    def extract_first_column(self, file_path):
        column_names = [] 
        try:
            with open(file_path, 'r') as file:
                for line in file:
                    parts = line.strip().split()
                    if parts:
                        column_names.append(parts[0])
        except FileNotFoundError:
            logging.error("The file %s does not exist.", file_path)
        except Exception as e:
            logging.error("An error occurred2: %s", e)
        return column_names

    def task_loop(self):
        if self.stop_event.is_set():
            logging.info("collect io thread exit")
            return

        for disk_name, stage_list in self.disk_map_stage.items():
            if self.get_blk_io_hierarchy(disk_name, stage_list) < 0:
                continue
            self.append_period_lat(disk_name, stage_list)

        threading.Timer(self.period_time, self.task_loop).start()

    def is_kernel_avaliable(self):
        base_path = '/sys/kernel/debug/block'
        all_disk = []
        for disk_name in os.listdir(base_path):
            disk_path = os.path.join(base_path, disk_name)
            blk_io_hierarchy_path = os.path.join(disk_path, 'blk_io_hierarchy')

            if not os.path.exists(blk_io_hierarchy_path):
                logging.error("no blk_io_hierarchy directory found in %s, skipping.", disk_name)
                continue

            for file_name in os.listdir(blk_io_hierarchy_path):
                file_path = os.path.join(blk_io_hierarchy_path, file_name)
                if file_name == 'stats':
                    all_disk.append(disk_name)

        for disk_name in self.disk_list:
            if not self.loop_all and disk_name not in all_disk:
                logging.warning("the %s disk not exist!", disk_name)
                continue
            stats_file = '/sys/kernel/debug/block/{}/blk_io_hierarchy/stats'.format(disk_name)
            stage_list = self.extract_first_column(stats_file)
            self.disk_map_stage[disk_name] = stage_list
            self.window_value[disk_name] = {}
            IO_GLOBAL_DATA[disk_name] = {}

        return len(IO_GLOBAL_DATA) != 0

    def main_loop(self):
        logging.info("collect io thread start")
        
        if not self.is_kernel_avaliable() or len(self.disk_map_stage) == 0:
            logging.warning("no disks meet the requirements. collect io thread exit")
            return

        for disk_name, stage_list in self.disk_map_stage.items():
            for stage in stage_list:
                self.window_value[disk_name][stage] = []
                IO_GLOBAL_DATA[disk_name][stage] = {}
                for category in Io_Category:
                    IO_GLOBAL_DATA[disk_name][stage][category] = []

        while True:
            start_time = time.time()

            if self.stop_event.is_set():
                logging.debug("collect io thread exit")
                return

            for disk_name, stage_list in self.disk_map_stage.items():
                if self.get_blk_io_hierarchy(disk_name, stage_list) < 0:
                    continue
                self.append_period_lat(disk_name, stage_list)
                
            elapsed_time = time.time() - start_time
            sleep_time = self.period_time - elapsed_time
            if sleep_time < 0:
                continue
            while sleep_time > 1:
                if self.stop_event.is_set():
                    logging.debug("collect io thread exit")
                    return
                time.sleep(1)
                sleep_time -= 1
            time.sleep(sleep_time)

    # set stop event, notify thread exit
    def stop_thread(self):
        self.stop_event.set()
