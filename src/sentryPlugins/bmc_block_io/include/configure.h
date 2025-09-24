/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_block_io is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * Author: hewanhan@h-partners.com
 */

#ifndef _BMCPLU_CONFIGURE_H_
#define _BMCPLU_CONFIGURE_H_

#include <string>

namespace BMCBlockIoPlu {

const std::string BMCPLU_CONFIG_PATH = "/etc/sysSentry/plugins/bmc_block_io.ini";
const std::string BMCPLU_LOG_PATH = "/var/log/sysSentry/bmc_block_io.log";
const std::string BMCPLU_MOD_CONFIG = "/etc/sysSentry/tasks/bmc_block_io.mod";
const int BMCPLU_PATROL_MIN = 60;
const int BMCPLU_PATROL_MAX = 3600;
const int BMCPLU_PATROL_DEFAULT = 300;
const int BMCPLU_CONFIG_CHECK_CYCLE = 10; // seconds
const int BMCPLU_DEFAULT_SLEEP_CYCLE = 3; // seconds
const int BMCPLU_LOGFILE_CHECK_CYCLE = 30; // second
const int BMCPLU_DEFAULT_ALARM_ID = 1002;
}
#endif
