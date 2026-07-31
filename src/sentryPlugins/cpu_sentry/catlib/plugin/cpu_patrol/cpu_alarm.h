/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: inspection message alarm program
 * Author: sxt1001
 * Create: 2026-07-28
 */

#ifndef CPU_ALARM_H
#define CPU_ALARM_H

#define PATH_REPORT_CPU_ALARM "/var/run/sysSentry/report.sock"
#define MAX_CHAR_LEN 128

enum report_module {
    CPU = 0x00
};
enum report_type {
    CE = 0x00,
    UCE = 0x01
};
enum report_trans_to {
    BMC = 0x01
};

enum report_event_type {
    ASSERTION = 0x00,
    DEASSERTION = 0x01
};

int cpu_alarm_Report(unsigned short type, unsigned short module, unsigned short trans_to, unsigned short command,
                     unsigned short event_type, int socket_id, int core_id);

#endif
