#!/bin/bash
# Copyright (c), 2026, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"

set +e

function pre_test() {
    dmesg -C

    [ ! -f /etc/sysSentry/tasks/sentry_msg_monitor.mod ] && yum install sentry_msg_monitor

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

    # 确认驱动插入成功
    lsmod | grep sentry
    expect_eq $? 0 "sentry ko insert failed"

    sentryctl set sentry_reporter --oom=on
    expect_eq $? 0 "enable oom event failed"

    value=$(cat /proc/sentry_reporter/oom | xargs)
    expect_str_eq "$value" "on"

    sentryctl set sentry_reporter --oom=off
    expect_eq $? 0 "disable oom event failed"

    value=$(cat /proc/sentry_reporter/oom | xargs)
    expect_str_eq "$value" "off"

    echo on > /proc/sentry_reporter/oom
    expect_eq $? 0 "enable oom event failed"

    value=$(cat /proc/sentry_reporter/oom | xargs)
    expect_str_eq "$value" "on"

    echo off > /proc/sentry_reporter/oom
    expect_eq $? 0 "disable oom event failed"

    value=$(cat /proc/sentry_reporter/oom | xargs)
    expect_str_eq "$value" "off"
}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    dmesg -C
}

run_testcase