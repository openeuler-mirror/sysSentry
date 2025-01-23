/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: hbm online repair core functions
 * Author: luckky
 * Create: 2024-10-30
 */

#ifndef __NON_STANDARD_HBM_REPAIR
#define __NON_STANDARD_HBM_REPAIR

#include "hbm-ras-handler.h"

#define DEFAULT_PAGE_SIZE_KB   4
#define HBM_MEM_RAS_NAME   "HISI0521"
#define HBM_UNKNOWN        0
#define HBM_HBM_MEMORY     1
#define HBM_DDR_MEMORY     2

#define TYPE_UINT32_WIDTH    32
#define HBM_REPAIR_REQ_TYPE  0
#define HBM_CE_ACLS          BIT(0)
#define HBM_PSUE_ACLS        BIT(1)
#define HBM_CE_SPPR          BIT(2)
#define HBM_PSUE_SPPR        BIT(3)
#define HBM_CACHE_MODE       (BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define HBM_ERROR_MASK       0b11111111
#define HBM_ADDL             1
#define HBM_ADDH             2
#define HBM_ERROR_TYPE_SIZE  4
#define HBM_ADDR_SIZE        8
#define HBM_ACLS_ADDR_NUM    1
#define HBM_SPPR_ADDR_NUM    16
#define HBM_ACLS_ARRAY_SIZE  (HBM_ERROR_TYPE_SIZE + HBM_ADDR_SIZE * HBM_ACLS_ADDR_NUM + HBM_ADDR_SIZE)
#define HBM_SPPR_ARRAY_SIZE  (HBM_ERROR_TYPE_SIZE + HBM_ADDR_SIZE * HBM_SPPR_ADDR_NUM + HBM_ADDR_SIZE)
#define HBM_CACHE_ARRAY_SIZE (HBM_ERROR_TYPE_SIZE + HBM_ADDR_SIZE)
#define HBMC_MODULE_ID              0x28
#define HBMC_SUBMOD_HBM_REPAIR      6
#define COMMON_VALID_MODULE_ID      5
#define COMMON_VALID_SUBMODULE_ID   6
#define COMMON_VALID_REG_ARRAY_SIZE 12

#define BMC_SOCKET_PATH    "/var/run/sysSentry/bmc.sock"
#define BMC_REPORT_FORMAT  "REP00%02x%02x%02x0000000000000000%02x%02x%02x00%02x00%02x%02x%02x%08x%08x0000000000"

#define ISOLATE_FAILED_OVER_THRESHOLD 0b10000001
#define ISOLATE_FAILED_OTHER_REASON   0b10000010
#define REPAIR_FAILED_NO_RESOURCE     0b10010100
#define REPAIR_FAILED_INVALID_PARAM   0b10011000
#define REPAIR_FAILED_OTHER_REASON    0b10011100
#define ONLINE_PAGE_FAILED            0b10100000
#define ISOLATE_REPAIR_ONLINE_SUCCESS 0b00000000

#define ROW_FAULT         1
#define SINGLE_ADDR_FAULT 6

enum fault_field_index {
    PROCESSOR_ID,
    DIE_ID,
    STACK_ID,
    SID,
    CHANNEL_ID,
    BANKGROUP_ID,
    BANK_ID,
    ROW_ID,
    COLUMN_ID,
    ERROR_TYPE,
    REPAIR_TYPE,
    RESERVED,
    CRC8,
    FAULT_FIELD_NUM
};

#define FAULT_ADDR_PROCESSOR_ID_LEN  2
#define FAULT_ADDR_DIE_ID_LEN        1
#define FAULT_ADDR_STACK_ID_LEN      3
#define FAULT_ADDR_SID_LEN           3
#define FAULT_ADDR_CHANNEL_ID_LEN    8
#define FAULT_ADDR_BANKGROUP_ID_LEN  3
#define FAULT_ADDR_BANK_ID_LEN       3
#define FAULT_ADDR_ROW_ID_LEN        17
#define FAULT_ADDR_COLUMN_ID_LEN     10
#define FAULT_ADDR_ERROR_TYPE_LEN    2
#define FAULT_ADDR_REPAIR_TYPE_LEN   2
#define FAULT_ADDR_RESERVED_LEN      2
#define FAULT_ADDR_CRC8_LEN          8

#define FAULT_ADDR_FIELD_MASK(field_len) ((1 << field_len) - 1)

#define EFI_VARIABLE_NON_VOLATILE       0x1
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x2
#define EFI_VARIABLE_RUNTIME_ACCESS     0x4
#define EFI_VARIABLE_APPEND_WRITE       0x40

#define EFIVARFS_PATH "/sys/firmware/efi/efivars"
#define MAX_VAR_SIZE (128 * 1024)
#define FLASH_ENTRY_NUM 8
#define KB_SIZE 1024

extern void get_flash_total_size();

#endif
