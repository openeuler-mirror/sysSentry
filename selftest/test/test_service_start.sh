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

    # 检查sysSentry安装包
    installed_syssentry_rpm=$(rpm -qa sysSentry)
    if [ -n "$installed_syssentry_rpm" ]; then
        yum remove -y sysSentry
    else
        make uninstall
    fi

    # 移除后重新加载systemd配置
    systemctl daemon-reload

    if [ -n "$installed_syssentry_rpm" ]; then
        yum install -y sentry_msg_monitor
    else
        make clean && make && make install
    fi

    # 安装后重新加载systemd配置
    systemctl daemon-reload

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