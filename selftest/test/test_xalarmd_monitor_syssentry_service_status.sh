#!/bin/bash
# Copyright (c), 2026, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"
source "libs/wait.sh"

set +e

tmp_log="tmp_log"

function pre_test() {
    # 编译ra_mock程序
    gcc test/RA_mock/RA_mock_delay.c -Wall -O2 -lxalarm -o test/RA_mock/RA_mock

    echo > /var/log/sysSentry/sysSentry.log
    echo > /var/log/sysSentry/xalarm.log

    systemctl restart xalarmd.socket sysSentry.socket
    systemctl restart sysSentry
    sleep 2
}

function do_test() {
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    test/RA_mock/RA_mock 0 0 > $tmp_log 2>&1 &

    kill -9 $(pgrep -f '/usr/bin/syssentry')

    wait_cmd_ok "systemctl is-active sysSentry | grep -E 'activating'" 1 3
    expect_eq $? 0 "sysSentry.service isn't activating as expect"

    sleep 1

    # 检测sysSentry服务变更广播消息以及xalarm接口返回错误码
    expect_true "grep -E 'sysSentry service state changed to' /var/log/sysSentry/xalarm.log"
    expect_true "grep -E 'sysSentry service is down, broadcasting alarm to all clients' /var/log/sysSentry/xalarm.log"
    expect_true "grep -E 'Broadcast sysSentry down msg success' /var/log/sysSentry/xalarm.log"
    expect_true "grep -E 'Failed to get msg, ret is \[-9\]' $tmp_log"

    # 等待服务自愈
    sleep 10
    expect_service_status_eq sysSentry active
}

function post_test() {
    kill -9 $(pgrep -f RA_mock) 2>/dev/null

    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    rm -rf ${tmp_log} test/RA_mock/RA_mock
}

run_testcase
