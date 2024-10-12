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

import os
import configparser
import logging

from .alarm_report import Report
from .threshold import ThresholdType
from .utils import get_threshold_type_enum, get_sliding_window_type_enum, get_log_level


LOG_FORMAT = "%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s"

ALL_STAGE_LIST = [
    "throtl",
    "wbt",
    "gettag",
    "plug",
    "deadline",
    "hctx",
    "requeue",
    "rq_driver",
    "bio",
]
ALL_IOTPYE_LIST = ["read", "write"]
DISK_TYPE_MAP = {
    0: "nvme_ssd",
    1: "sata_ssd",
    2: "sata_hdd",
}


def init_log_format(log_level: str):
    logging.basicConfig(level=get_log_level(log_level.lower()), format=LOG_FORMAT)
    if log_level.lower() not in ("info", "warning", "error", "debug"):
        logging.warning(
            "the log_level: %s you set is invalid, use default value: info.", log_level
        )


class ConfigParser:
    DEFAULT_CONF = {
        "log": {"level": "info"},
        "common": {
            "slow_io_detect_frequency": 1,
            "disk": None,
            "stage": "throtl,wbt,gettag,plug,deadline,hctx,requeue,rq_driver,bio",
            "iotype": "read,write",
        },
        "algorithm": {
            "train_data_duration": 24.0,
            "train_update_duration": 2.0,
            "algorithm_type": get_threshold_type_enum("boxplot"),
            "boxplot_parameter": 1.5,
            "n_sigma_parameter": 3.0,
        },
        "sliding_window": {
            "sliding_window_type": get_sliding_window_type_enum("not_continuous"),
            "window_size": 30,
            "window_minimum_threshold": 6,
        },
        "latency_sata_ssd": {"read_tot_lim": 50000, "write_tot_lim": 50000},
        "latency_nvme_ssd": {"read_tot_lim": 500, "write_tot_lim": 500},
        "latency_sata_hdd": {"read_tot_lim": 50000, "write_tot_lim": 50000},
    }

    def __init__(self, config_file_name):
        self._conf = ConfigParser.DEFAULT_CONF
        self._config_file_name = config_file_name

    def _get_config_value(
        self,
        config_items: dict,
        key: str,
        value_type,
        default_value=None,
        gt=None,
        ge=None,
        lt=None,
        le=None,
    ):
        value = config_items.get(key)
        if value is None:
            logging.warning(
                "config of %s not found, the default value %s will be used.",
                key,
                default_value,
            )
            value = default_value
        if not value:
            logging.critical(
                "the value of %s is empty, ai_block_io plug will exit.", key
            )
            Report.report_pass(
                f"the value of {key} is empty, ai_block_io plug will exit."
            )
            exit(1)
        try:
            value = value_type(value)
        except ValueError:
            logging.critical(
                "the value of %s is not a valid %s, ai_block_io plug will exit.",
                key,
                value_type,
            )
            Report.report_pass(
                f"the value of {key} is not a valid {value_type}, ai_block_io plug will exit."
            )
            exit(1)
        if gt is not None and value <= gt:
            logging.critical(
                "the value of %s is not greater than %s, ai_block_io plug will exit.",
                key,
                gt,
            )
            Report.report_pass(
                f"the value of {key} is not greater than {gt}, ai_block_io plug will exit."
            )
            exit(1)
        if ge is not None and value < ge:
            logging.critical(
                "the value of %s is not greater than or equal to %s, ai_block_io plug will exit.",
                key,
                ge,
            )
            Report.report_pass(
                f"the value of {key} is not greater than or equal to {ge}, ai_block_io plug will exit."
            )
            exit(1)
        if lt is not None and value >= lt:
            logging.critical(
                "the value of %s is not less than %s, ai_block_io plug will exit.",
                key,
                lt,
            )
            Report.report_pass(
                f"the value of {key} is not less than {lt}, ai_block_io plug will exit."
            )
            exit(1)
        if le is not None and value > le:
            logging.critical(
                "the value of %s is not less than or equal to %s, ai_block_io plug will exit.",
                key,
                le,
            )
            Report.report_pass(
                f"the value of {key} is not less than or equal to {le}, ai_block_io plug will exit."
            )
            exit(1)

        return value

    def _read_slow_io_detect_frequency(self, items_common: dict):
        self._conf["common"]["slow_io_detect_frequency"] = self._get_config_value(
            items_common,
            "slow_io_detect_frequency",
            int,
            self.DEFAULT_CONF["common"]["slow_io_detect_frequency"],
            gt=0,
            le=300,
        )

    def _read_disks_to_detect(self, items_common: dict):
        disks_to_detection = items_common.get("disk")
        if disks_to_detection is None:
            logging.warning("config of disk not found, the default value will be used.")
            self._conf["common"]["disk"] = None
            return
        disks_to_detection = disks_to_detection.strip()
        if not disks_to_detection:
            logging.critical("the value of disk is empty, ai_block_io plug will exit.")
            Report.report_pass(
                "the value of disk is empty, ai_block_io plug will exit."
            )
            exit(1)
        disk_list = disks_to_detection.split(",")
        disk_list = [disk.strip() for disk in disk_list]
        if len(disk_list) == 1 and disk_list[0] == "default":
            self._conf["common"]["disk"] = None
            return
        self._conf["common"]["disk"] = disk_list

    def _read_train_data_duration(self, items_algorithm: dict):
        self._conf["common"]["train_data_duration"] = self._get_config_value(
            items_algorithm,
            "train_data_duration",
            float,
            self.DEFAULT_CONF["algorithm"]["train_data_duration"],
            gt=0,
            le=720,
        )

    def _read_train_update_duration(self, items_algorithm: dict):
        default_train_update_duration = self.DEFAULT_CONF["algorithm"][
            "train_update_duration"
        ]
        if default_train_update_duration > self._conf["common"]["train_data_duration"]:
            default_train_update_duration = (
                self._conf["common"]["train_data_duration"] / 2
            )
        self._conf["common"]["train_update_duration"] = self._get_config_value(
            items_algorithm,
            "train_update_duration",
            float,
            default_train_update_duration,
            gt=0,
            le=self._conf["common"]["train_data_duration"],
        )

    def _read_algorithm_type_and_parameter(self, items_algorithm: dict):
        algorithm_type = items_algorithm.get("algorithm_type")
        if algorithm_type is not None:
            self._conf["algorithm"]["algorithm_type"] = get_threshold_type_enum(
                algorithm_type
            )
        if self._conf["algorithm"]["algorithm_type"] is None:
            logging.critical(
                "the algorithm_type: %s you set is invalid. ai_block_io plug will exit.",
                algorithm_type,
            )
            Report.report_pass(
                f"the algorithm_type: {algorithm_type} you set is invalid. ai_block_io plug will exit."
            )
            exit(1)
        elif self._conf["algorithm"]["algorithm_type"] == ThresholdType.NSigmaThreshold:
            self._conf["algorithm"]["n_sigma_parameter"] = self._get_config_value(
                items_algorithm,
                "n_sigma_parameter",
                float,
                self.DEFAULT_CONF["algorithm"]["n_sigma_parameter"],
                gt=0,
                le=10,
            )
        elif (
            self._conf["algorithm"]["algorithm_type"] == ThresholdType.BoxplotThreshold
        ):
            self._conf["algorithm"]["boxplot_parameter"] = self._get_config_value(
                items_algorithm,
                "boxplot_parameter",
                float,
                self.DEFAULT_CONF["algorithm"]["boxplot_parameter"],
                gt=0,
                le=10,
            )

    def _read_stage(self, items_algorithm: dict):
        stage_str = items_algorithm.get(
            "stage", self.DEFAULT_CONF["common"]["stage"]
        ).strip()
        stage_list = stage_str.split(",")
        stage_list = [stage.strip() for stage in stage_list]
        if len(stage_list) == 1 and stage_list[0] == "":
            logging.critical("stage value not allow is empty, exiting...")
            exit(1)
        if len(stage_list) == 1 and stage_list[0] == "default":
            logging.warning(
                "stage will enable default value: %s",
                self.DEFAULT_CONF["common"]["stage"],
            )
            self._conf["common"]["stage"] = ALL_STAGE_LIST
            return
        for stage in stage_list:
            if stage not in ALL_STAGE_LIST:
                logging.critical(
                    "stage: %s is not valid stage, ai_block_io will exit...", stage
                )
                exit(1)
        dup_stage_list = set(stage_list)
        if "bio" not in dup_stage_list:
            logging.critical("stage must contains bio stage, exiting...")
            exit(1)
        self._conf["common"]["stage"] = dup_stage_list

    def _read_iotype(self, items_algorithm: dict):
        iotype_str = items_algorithm.get(
            "iotype", self.DEFAULT_CONF["common"]["iotype"]
        ).strip()
        iotype_list = iotype_str.split(",")
        iotype_list = [iotype.strip() for iotype in iotype_list]
        if len(iotype_list) == 1 and iotype_list[0] == "":
            logging.critical("iotype value not allow is empty, exiting...")
            exit(1)
        if len(iotype_list) == 1 and iotype_list[0] == "default":
            logging.warning(
                "iotype will enable default value: %s",
                self.DEFAULT_CONF["common"]["iotype"],
            )
            self._conf["common"]["iotype"] = ALL_IOTPYE_LIST
            return
        for iotype in iotype_list:
            if iotype not in ALL_IOTPYE_LIST:
                logging.critical(
                    "iotype: %s is not valid iotype, ai_block_io will exit...", iotype
                )
                exit(1)
        dup_iotype_list = set(iotype_list)
        self._conf["common"]["iotype"] = dup_iotype_list

    def _read_sliding_window_type(self, items_sliding_window: dict):
        sliding_window_type = items_sliding_window.get("sliding_window_type")
        if sliding_window_type is not None:
            self._conf["sliding_window"]["sliding_window_type"] = (
                get_sliding_window_type_enum(sliding_window_type)
            )
        if self._conf["sliding_window"]["sliding_window_type"] is None:
            logging.critical(
                "the sliding_window_type: %s you set is invalid. ai_block_io plug will exit.",
                sliding_window_type,
            )
            Report.report_pass(
                f"the sliding_window_type: {sliding_window_type} you set is invalid. ai_block_io plug will exit."
            )
            exit(1)

    def _read_window_size(self, items_sliding_window: dict):
        self._conf["sliding_window"]["window_size"] = self._get_config_value(
            items_sliding_window,
            "window_size",
            int,
            self.DEFAULT_CONF["sliding_window"]["window_size"],
            gt=0,
            le=3600,
        )

    def _read_window_minimum_threshold(self, items_sliding_window: dict):
        default_window_minimum_threshold = self.DEFAULT_CONF["sliding_window"][
            "window_minimum_threshold"
        ]
        if (
            default_window_minimum_threshold
            > self._conf["sliding_window"]["window_size"]
        ):
            default_window_minimum_threshold = (
                self._conf["sliding_window"]["window_size"] / 2
            )
        self._conf["sliding_window"]["window_minimum_threshold"] = (
            self._get_config_value(
                items_sliding_window,
                "window_minimum_threshold",
                int,
                default_window_minimum_threshold,
                gt=0,
                le=self._conf["sliding_window"]["window_size"],
            )
        )

    def read_config_from_file(self):
        if not os.path.exists(self._config_file_name):
            init_log_format(self._conf["log"]["level"])
            logging.critical(
                "config file %s not found, ai_block_io plug will exit.",
                self._config_file_name,
            )
            Report.report_pass(
                f"config file {self._config_file_name} not found, ai_block_io plug will exit."
            )
            exit(1)

        con = configparser.ConfigParser()
        try:
            con.read(self._config_file_name, encoding="utf-8")
        except configparser.Error as e:
            init_log_format(self._conf["log"]["level"])
            logging.critical(
                "config file read error: %s, ai_block_io plug will exit.", e
            )
            Report.report_pass(
                f"config file read error: {e}, ai_block_io plug will exit."
            )
            exit(1)

        if con.has_section("log"):
            items_log = dict(con.items("log"))
            # 情况一：没有log，则使用默认值
            # 情况二：有log，值为空或异常，使用默认值
            # 情况三：有log，值正常，则使用该值
            self._conf["log"]["level"] = items_log.get(
                "level", self.DEFAULT_CONF["log"]["level"]
            )
            init_log_format(self._conf["log"]["level"])
        else:
            init_log_format(self._conf["log"]["level"])
            logging.warning(
                "log section parameter not found, it will be set to default value."
            )

        if con.has_section("common"):
            items_common = dict(con.items("common"))

            self._read_slow_io_detect_frequency(items_common)
            self._read_disks_to_detect(items_common)
            self._read_stage(items_common)
            self._read_iotype(items_common)
        else:
            logging.warning(
                "common section parameter not found, it will be set to default value."
            )

        if con.has_section("algorithm"):
            items_algorithm = dict(con.items("algorithm"))
            self._read_train_data_duration(items_algorithm)
            self._read_train_update_duration(items_algorithm)
            self._read_algorithm_type_and_parameter(items_algorithm)
        else:
            logging.warning(
                "algorithm section parameter not found, it will be set to default value."
            )

        if con.has_section("sliding_window"):
            items_sliding_window = dict(con.items("sliding_window"))

            self._read_window_size(items_sliding_window)
            self._read_window_minimum_threshold(items_sliding_window)
        else:
            logging.warning(
                "sliding_window section parameter not found, it will be set to default value."
            )

        if con.has_section("latency_sata_ssd"):
            items_latency_sata_ssd = dict(con.items("latency_sata_ssd"))
            self._conf["latency_sata_ssd"]["read_tot_lim"] = self._get_config_value(
                items_latency_sata_ssd,
                "read_tot_lim",
                int,
                self.DEFAULT_CONF["latency_sata_ssd"]["read_tot_lim"],
                gt=0,
            )
            self._conf["latency_sata_ssd"]["write_tot_lim"] = self._get_config_value(
                items_latency_sata_ssd,
                "write_tot_lim",
                int,
                self.DEFAULT_CONF["latency_sata_ssd"]["write_tot_lim"],
                gt=0,
            )
        else:
            logging.warning(
                "latency_sata_ssd section parameter not found, it will be set to default value."
            )
        if con.has_section("latency_nvme_ssd"):
            items_latency_nvme_ssd = dict(con.items("latency_nvme_ssd"))
            self._conf["latency_nvme_ssd"]["read_tot_lim"] = self._get_config_value(
                items_latency_nvme_ssd,
                "read_tot_lim",
                int,
                self.DEFAULT_CONF["latency_nvme_ssd"]["read_tot_lim"],
                gt=0,
            )
            self._conf["latency_nvme_ssd"]["write_tot_lim"] = self._get_config_value(
                items_latency_nvme_ssd,
                "write_tot_lim",
                int,
                self.DEFAULT_CONF["latency_nvme_ssd"]["write_tot_lim"],
                gt=0,
            )
        else:
            logging.warning(
                "latency_nvme_ssd section parameter not found, it will be set to default value."
            )
        if con.has_section("latency_sata_hdd"):
            items_latency_sata_hdd = dict(con.items("latency_sata_hdd"))
            self._conf["latency_sata_hdd"]["read_tot_lim"] = self._get_config_value(
                items_latency_sata_hdd,
                "read_tot_lim",
                int,
                self.DEFAULT_CONF["latency_sata_hdd"]["read_tot_lim"],
                gt=0,
            )
            self._conf["latency_sata_hdd"]["write_tot_lim"] = self._get_config_value(
                items_latency_sata_hdd,
                "write_tot_lim",
                int,
                self.DEFAULT_CONF["latency_sata_hdd"]["write_tot_lim"],
                gt=0,
            )
        else:
            logging.warning(
                "latency_sata_hdd section parameter not found, it will be set to default value."
            )

        self.__print_all_config_value()

    def __repr__(self) -> str:
        return str(self._conf)

    def __str__(self) -> str:
        return str(self._conf)

    def __print_all_config_value(self):
        logging.info("all config is follow:\n %s", self)

    def get_tot_lim(self, disk_type, io_type):
        if io_type == "read":
            return self._conf.get(
                f"latency_{DISK_TYPE_MAP.get(disk_type, '')}", {}
            ).get("read_tot_lim", None)
        elif io_type == "write":
            return self._conf.get(
                f"latency_{DISK_TYPE_MAP.get(disk_type, '')}", {}
            ).get("write_tot_lim", None)
        else:
            return None

    def get_train_data_duration_and_train_update_duration(self):
        return (
            self._conf["common"]["train_data_duration"],
            self._conf["common"]["train_update_duration"],
        )

    def get_window_size_and_window_minimum_threshold(self):
        return (
            self._conf["sliding_window"]["window_size"],
            self._conf["sliding_window"]["window_minimum_threshold"],
        )

    @property
    def slow_io_detect_frequency(self):
        return self._conf["common"]["slow_io_detect_frequency"]

    @property
    def algorithm_type(self):
        return self._conf["algorithm"]["algorithm_type"]

    @property
    def sliding_window_type(self):
        return self._conf["sliding_window"]["sliding_window_type"]

    @property
    def train_data_duration(self):
        return self._conf["common"]["train_data_duration"]

    @property
    def train_update_duration(self):
        return self._conf["common"]["train_update_duration"]

    @property
    def window_size(self):
        return self._conf["sliding_window"]["window_size"]

    @property
    def window_minimum_threshold(self):
        return self._conf["sliding_window"]["window_minimum_threshold"]

    @property
    def absolute_threshold(self):
        return self._conf["common"]["absolute_threshold"]

    @property
    def log_level(self):
        return self._conf["log"]["level"]

    @property
    def disks_to_detection(self):
        return self._conf["common"]["disk"]

    @property
    def stage(self):
        return self._conf["common"]["stage"]

    @property
    def iotype(self):
        return self._conf["common"]["iotype"]

    @property
    def boxplot_parameter(self):
        return self._conf["algorithm"]["boxplot_parameter"]

    @property
    def n_sigma_parameter(self):
        return self._conf["algorithm"]["n_sigma_parameter"]
