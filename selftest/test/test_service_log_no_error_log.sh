#!/bin/bash
# Copyright (c), 2026, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"

set +e


function pre_test() {
    # 如果环境中有启动ubse服务，需要关闭，否则一些版本中会影响该用例
    systemctl stop ubse

    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    echo > /var/log/sysSentry/sysSentry.log
    echo > /var/log/sysSentry/xalarm.log

    systemctl start xalarmd.socket sysSentry.socket
    systemctl start sysSentry
    sleep 2
}

function do_test() {
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    # 服务运行一段时间
    sleep 30

    # 检测sysSentry服务和xalarmd服务的日志中无ERROR级别的日志
    expect_false "grep -E -- '- ERROR -' /var/log/sysSentry/xalarm.log"
    expect_false "grep -E -- '- ERROR -' /var/log/sysSentry/sysSentry.log"

    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    # 停止服务后，再次检测是否有ERROR级别的日志
    expect_false "grep -E -- '- ERROR -' /var/log/sysSentry/xalarm.log"
    expect_false "grep -E -- '- ERROR -' /var/log/sysSentry/sysSentry.log"
}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null
}

run_testcase
