/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * sysSentry is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.

 * Description: header file for ub mem event
 * Author: sxt1001
 * Create: 2025-11-10
*/

#ifndef SENTRY_UB_FAULT_LIB_H
#define SENTRY_UB_FAULT_LIB_H

#include <stdint.h>
#include <libobmm.h>

#define MAX_PATH 128
#define PAGE_SIZE 4096
#define FD_MODE 0
#define NUMA_MODE 1

enum sentry_ubus_mem_err_type {
    SENTRY_MEM_ERR_ROUTE,
    SENTRY_MEM_FLUX_INT,
    SENTRY_MEM_ERR_OUTBOUND_TRANSLATION,
    SENTRY_MEM_ERR_INBOUND_TRANSLATION,
    SENTRY_MEM_ERR_TIMEOUT,
    SENTRY_MEM_ERR_BUS,
    SENTRY_MEM_ERR_UCE,
    SENTRY_MEM_ERR_NO_REPORT = 1000,
};

int find_and_send_sigbus_to_thread(mem_id memid, unsigned long obmm_offset);
#endif
