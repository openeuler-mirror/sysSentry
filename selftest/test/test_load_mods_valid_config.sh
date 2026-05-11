#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"
set +e

tmp_log="tmp_log"
start_line=0
mod_name="test_valid_config"

# 测试所有配置合法时的正常加载行为

function pre_test() {
    start_line=$(expr $(wc -l < /var/log/sysSentry/sysSentry.log) + 1)

    kill -9 `ps aux|grep syssentry|grep -v grep|awk '{print $2}'`
    kill -9 `ps aux|grep test_task|grep -v grep|awk '{print $2}'`

    # 创建合法的 env_file
    env_file="/etc/sysSentry/plugins/$mod_name.env"
    echo "TEST_VAR=valid_value" > "$env_file"

    config_file="/etc/sysSentry/tasks/$mod_name.mod"
    if [ -f $config_file ]; then
        rm -rf $config_file
    fi

    touch "$config_file"
    echo "[common]" > "$config_file"
    echo "enabled=yes" >> "$config_file"
    echo "task_start=test_task" >> "$config_file"
    echo "task_stop=pkill test_task" >> "$config_file"
    echo "type=oneshot" >> "$config_file"
    echo "alarm_id=1016" >> "$config_file"
    echo "alarm_clear_time=30" >> "$config_file"
    echo "env_file=$env_file" >> "$config_file"
    echo "conflict=down" >> "$config_file"

    gcc test/sysSentry/test_task.c -o test/sysSentry/test_task
    cp test/sysSentry/test_task /usr/bin

    systemctl start xalarmd.socket xalarmd.service
    systemctl start sysSentry.socket sysSentry.service
    sleep 2
}

function do_test() {
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    end_line=$(wc -l < /var/log/sysSentry/sysSentry.log)
    sed -n "${start_line}, ${end_line}p" /var/log/sysSentry/sysSentry.log > ${tmp_log}

    # 检查日志中不应该有校验失败的错误信息
    expect_false "grep -E 'mod $mod_name: alarm_id must be' ${tmp_log}"
    expect_false "grep -E 'mod $mod_name: alarm_clear_time must be' ${tmp_log}"
    expect_false "grep -E 'mod $mod_name: env_file.*does not exist' ${tmp_log}"
    expect_false "grep -E 'mod $mod_name: conflict must be one of' ${tmp_log}"

    # 检查任务是否加载成功
    expect_task_status_eq "$mod_name" "EXITED"

    # 检查任务能够正常启动
    sentryctl start "$mod_name"
    expect_eq $? 0

    sleep 1

    expect_task_status_eq "$mod_name" "RUNNING"

    sentryctl stop "$mod_name"
    expect_eq $? 0

    expect_task_status_eq "$mod_name" "FAILED"
}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service
    systemctl stop xalarmd.socket xalarmd.service
    kill -9 `pgrep -w test_task`
    rm -rf ${tmp_log} test/sysSentry/test_task /usr/bin/test_task
    rm -rf /etc/sysSentry/tasks/$mod_name.mod /etc/sysSentry/plugins/$mod_name.env
}

run_testcase