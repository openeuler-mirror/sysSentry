#!/bin/bash
# Copyright (c), 2026, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"

set +e

function pre_test() {
    dmesg -C

    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null
    sleep 2
}

# 测试软件包卸载重装服务启动成功场景

function do_test() {
    systemctl start xalarmd.socket xalarmd.service
    expect_eq $? 0 "xalarmd service start failed"

    systemctl start sysSentry.socket sysSentry.service
    expect_eq $? 0 "sysSentry service start failed"

    # 检测服务是否启动成功
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    sleep 3

    yum remove sysSentry -y

    yum install sysSentry sentry_msg_monitor -y

    systemctl start xalarmd.socket xalarmd.service
    expect_eq $? 0 "xalarmd service start failed"

    systemctl start sysSentry.socket sysSentry.service
    expect_eq $? 0 "sysSentry service start failed"

    # 检测服务是否启动成功
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active
}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null
}

run_testcase