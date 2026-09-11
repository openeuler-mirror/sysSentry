#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/wait.sh"
source "libs/shopt.sh"
set +e

# 看护 xalarmd 在 wait_for_connection 中等待客户端注册消息的超时行为：
#   1. 源码常量 REG_MSG_TIMEOUT 保持为 0.05（50ms），防止回归到历史上导致告警丢失的 0.5（500ms）。
#   2. 老 API 客户端（xalarm_Register，不发注册消息）经过超时后被作为纯告警接收方加入转发表，
#      且能正常收到告警 —— 回归 500ms 窗口内告警被静默丢弃的问题。
#   3. 新 API 客户端（xalarm_register_event，connect 后同步发送注册 JSON）在 50ms 内完成注册，
#      服务端日志出现 "registered event"，证明 50ms 对同步发送的注册消息足够。

XALARM_LOG="/var/log/sysSentry/xalarm.log"
TRANSFER_SRC="../src/services/xalarm/xalarm_transfer.py"

function pre_test() {
    rm -rf ./checklog ./tmp_log test/xalarm/reg_demo test/xalarm/send_demo test/RA_mock/RA_mock
    gcc test/xalarm/reg_demo.c -o test/xalarm/reg_demo -lxalarm
    gcc test/xalarm/send_demo.c -o test/xalarm/send_demo -lxalarm
    gcc test/RA_mock/RA_mock_delay.c -Wall -O2 -lxalarm -o test/RA_mock/RA_mock
    systemctl stop xalarmd.socket xalarmd.service
    sleep 1
    systemctl start xalarmd.socket xalarmd.service
    for i in $(seq 1 10); do
        if [ "$(systemctl is-active xalarmd.service)" = "active" ]; then
            break
        fi
        sleep 1
    done
    sleep 1
    echo > "$XALARM_LOG"
}

function do_test() {
    # 场景一：源码常量看护 —— REG_MSG_TIMEOUT 必须为 0.05，防止回归到 500ms
    expect_true "grep -q '^REG_MSG_TIMEOUT = 0.05$' $TRANSFER_SRC" \
        "REG_MSG_TIMEOUT is 0.05 (50ms) in source"

    # 场景二：老 API 客户端不发注册消息，超时后被保留为告警接收方，且能收到告警
    ./test/xalarm/reg_demo >> checklog 2>&1 &
    wait_cmd_ok "grep \"register success\" ./checklog" 1 5
    expect_eq $? 0 "legacy api register success"
    # 等待服务端走完超时分支（50ms）并把连接加入 fd_to_socket 转发表
    sleep 1

    ./test/xalarm/send_demo 1001 1 2 "register timeout legacy alarm" >> checklog 2>&1
    wait_cmd_ok "grep \"register timeout legacy alarm\" ./checklog" 1 5
    expect_eq $? 0 "legacy api client received alarm after timeout window"

    # 服务端日志应记录超时分支命中，且超时值日志显示 50ms（非 500ms）
    expect_true "grep -q 'does not send registration message in 50 ms' $XALARM_LOG" \
        "server logged legacy client 50ms timeout (not 500ms)"

    # 等待 reg_demo 内部 sleep(10) 结束后打印解注册成功（reg_demo.c SLEEP_TIME=10）
    sleep 10
    wait_cmd_ok "grep \"unregister xalarm success\" ./checklog" 1 5
    expect_eq $? 0 "legacy api unregister success"

    # 场景三：新 API 客户端 connect 后同步发送注册 JSON，在 50ms 内完成注册
    echo > "$XALARM_LOG"
    ./test/RA_mock/RA_mock > tmp_log 2>&1 &
    # RA_mock_delay.c 用 printf 但无 fflush，重定向到文件时 stdout 为全缓冲，
    # "Waiting for plugin msg" 会滞留缓冲区；改为看护服务端日志的 "registered event"
    wait_cmd_ok "grep -q 'registered event' $XALARM_LOG" 1 5
    expect_eq $? 0 "new api client registered event within 50ms timeout"

    kill -9 $(pgrep -f RA_mock) 2>/dev/null
}

function post_test() {
    kill -9 $(pgrep -f RA_mock) 2>/dev/null
    kill -9 $(pgrep -f reg_demo) 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service
    sleep 1
    cat ./checklog 2>/dev/null
    cat ./tmp_log 2>/dev/null
    rm -rf ./checklog ./tmp_log test/xalarm/reg_demo test/xalarm/send_demo test/RA_mock/RA_mock
    systemctl stop xalarmd.socket xalarmd.service
    sleep 1
}

run_testcase
