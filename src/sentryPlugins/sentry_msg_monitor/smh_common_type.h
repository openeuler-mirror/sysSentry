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

 * Description: header file for sentry msg monitor
 * Author: Luckky
 * Create: 2025-02-18
*/

#ifndef SMH_COMMON_TYPE_H
#define SMH_COMMON_TYPE_H

#include <stdint.h>

#define SMH_TYPE ('}')
#define OOM_EVENT_MAX_NUMA_NODES 8
#define EID_MAX_LEN 40 // eid str len 39 + '\0'

/* ---- version query response ---- */
struct smh_version_info {
    uint32_t major;           /* sizeof(struct sentry_msg_helper_msg) */
    uint32_t minor;           /* highest value in sentry_msg_helper_msg_type enum */
};


/*
 * The main version of the sentry driver.
 * It increases by 1 whenever there is an interface change in the Sentry driver or sysSentry package
 */
#define SMH_VERSION_MAJOR  1
/*
 * Defined as the number of message types supported for reporting by the sentry driver.
 * The current Sentry driver supports reporting the following types of abnormal event msg:
 * (1) panic
 * (2) reboot
 * (3) poweroff
 * (4) oom
 * (5) ub mem err
 * (6) ub link
 */
#define SMH_VERSION_MINOR  6

enum {
    SMH_CMD_MSG_ACK = 0x10,
    SMH_CMD_VERSION_CHECK = 0x11,
};

#define SMH_MSG_ACK _IO(SMH_TYPE, SMH_CMD_MSG_ACK)
#define SMH_VERSION_CHECK  _IOR(SMH_TYPE, SMH_CMD_VERSION_CHECK, struct smh_version_info)

/*
 * enum ras_err_type — mirror of the kernel's ub-mem-decoder.h values.
 * This enum must be kept in sync with the kernel header.  Any divergence
 * will cause convert_ubus_type_to_sentry_type() to misclassify or silently
 * drop new error types.
 */
enum ras_err_type {
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR,
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR,
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
};

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

/* ---- message types (enum values are the ABI) ---- */
enum sentry_msg_helper_msg_type {
    SMH_MESSAGE_POWER_OFF,
    SMH_MESSAGE_OOM,
    SMH_MESSAGE_PANIC,
    SMH_MESSAGE_KERNEL_REBOOT,
    SMH_MESSAGE_UB_MEM_ERR,
    SMH_MESSAGE_PANIC_ACK,
    SMH_MESSAGE_KERNEL_REBOOT_ACK,
    SMH_MESSAGE_HEARTBEAT,
    SMH_MESSAGE_HEARTBEAT_ACK,
    SMH_MESSAGE_LINK_EVENT,
    SMH_MESSAGE_UNKNOWN,
};

/* ---- the primary kernel↔userspace data structure ---- */
struct sentry_msg_helper_msg {
    enum sentry_msg_helper_msg_type type;
    uint64_t msgid;
    uint64_t start_send_time;
    uint64_t timeout_time;
    // reboot_info is empty
    union {
        struct {
            int nr_nid;
            int nid[OOM_EVENT_MAX_NUMA_NODES];
            int sync;
            int timeout;
            int reason;
        } oom_info;
        struct {
            uint32_t cna;
            char eid[EID_MAX_LEN];
        } remote_info;
        struct {
            uint64_t pa;
            int mem_type;
            int fault_with_kill;
            enum ras_err_type raw_ubus_mem_err_type;
        } ub_mem_info;
        struct {
            uint16_t port_id;
            unsigned int scna;
            int link_event;
        } link_info;
    } helper_msg_info;
    unsigned long res;
};

#endif
