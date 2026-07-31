/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: inspection message alarm program
 * Author: sxt1001
 * Create: 2026-07-28
 */

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "io_utils.h"
#include "cpu_alarm.h"

static int init_report_addr(struct sockaddr_un *alarm_addr, const char *report_path)
{
    if (alarm_addr == NULL) {
        fprintf(stderr, "%s: alarm_addr is null\n", __func__);
        return -1;
    }

    memset(alarm_addr, 0, sizeof(struct sockaddr_un));
    alarm_addr->sun_family = AF_UNIX;
    int ret = snprintf(alarm_addr->sun_path, sizeof(alarm_addr->sun_path), "%s", report_path);
    if (ret < 0 || ret >= sizeof(alarm_addr->sun_path)) {
        fprintf(stderr, "%s: snprintf failed\n", __func__);
        return -1;
    }
    return 0;
}

static bool is_valid_report_module(unsigned short module) {
    switch ((int) module) {
        case CPU:
            return true;
        default:
            return false;
    }
}

static bool is_valid_report_type(unsigned short type) {
    switch ((int) type) {
        case CE:
        case UCE:
            return true;
        default:
            return false;
    }
}

static bool is_valid_report_trans_to(unsigned short trans_to) {
    switch ((int) trans_to) {
        case BMC:
            return true;
        default:
            return false;
    }
}

static bool check_params(unsigned short type, unsigned short module, unsigned short trans_to, int report_info_len) {
    bool is_valid_type = is_valid_report_type(type);
    bool is_valid_module = is_valid_report_module(module);
    bool is_valid_trans_to = is_valid_report_trans_to(trans_to);
    bool is_valid_report_info_len = (report_info_len >= 0 && report_info_len <= 999) ? true : false;

    return is_valid_type && is_valid_module && is_valid_trans_to && is_valid_report_info_len;
}

int cpu_alarm_Report(unsigned short type, unsigned short module, unsigned short trans_to, unsigned short command,
                     unsigned short event_type, int socket_id, int core_id)
{
    int fd;
    ssize_t ret;
    bool is_valid;
    int report_info_len, alarm_msg_len;
    char report_info[MAX_CHAR_LEN];
    char alarm_msg[MAX_CHAR_LEN];
    struct sockaddr_un alarm_addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "%s socket create error: %s\n", __func__, strerror(errno));
        return -1;
    }

    ret = init_report_addr(&alarm_addr, PATH_REPORT_CPU_ALARM);
    if (ret == -1) {
        close(fd);
        return -1;
    }

    report_info_len = snprintf(report_info, sizeof(report_info), "%u %u %d %d", command, event_type, socket_id, core_id);
    if (report_info_len < 0 || report_info_len >= (int)sizeof(report_info)) {
        fprintf(stderr, "%s: failed to format report_info\n", __func__);
        close(fd);
        return -1;
    }
    is_valid = check_params(type, module, trans_to, report_info_len);
    if (!is_valid) {
        fprintf(stderr, "%s: cpu_alarm: invalid params\n", __func__);
        close(fd);
        return -1;
    }

    alarm_msg_len = snprintf(alarm_msg, sizeof(alarm_msg), "REP%1u%1u%02u%03d%s",
                type, module, trans_to, report_info_len, report_info);
    if (alarm_msg_len < 0 || alarm_msg_len >= (int)sizeof(alarm_msg)) {
        fprintf(stderr, "%s: failed to format alarm_msg\n", __func__);
        close(fd);
        return -1;
    }

    while (true) {
        ret = connect(fd, (struct sockaddr *)&alarm_addr, offsetof(struct sockaddr_un, sun_path) + strlen(alarm_addr.sun_path));

        if (ret < 0) {
            if (errno == EINTR) {
                /* interrupted by signal, ignore */
                continue;
            } else {
                fprintf(stderr, "%s: connect failed errno: %d\n", __func__, errno);
                break;
            }
        }

        ret = WriteAll(fd, alarm_msg, strlen(alarm_msg));
        if (ret < 0) {
            fprintf(stderr, "%s: write failed errno: %d\n", __func__, errno);
        }
        break;
    }
    close(fd);

    return (ret > 0) ? 0 : -1;
}
