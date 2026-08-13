#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"
set +e

tmp_log="tmp_log"

#未启动守护进程时，sentryctl执行是否正确处理异常
#sentryctl 一个命令字后带多个变参
#sentryctl 多个命令字同时使用
#sentryctl 使用错误的变参
#sentryctl 使用非法的命令字
#配置文件重载（修改任务类型）

function pre_test() {
    kill -9 `ps aux|grep test_task|grep -v grep|awk '{print $2}'`

    add_test_config "test_task" "pkill test_task" "oneshot" 60 3600 "test_sentryctl_exception"
    gcc test/sysSentry/test_task.c -o test/sysSentry/test_task
    cp test/sysSentry/test_task /usr/bin

    systemctl stop sysSentry.socket sysSentry.service
    systemctl stop xalarmd.socket xalarmd.service

    sleep 2
}

function do_test() {
    systemctl start xalarmd.socket xalarmd.service

    # 下面5个测试sentryctl命令执行失败场景的检测代码需要sysSentry服务是未启动的
    expect_service_status_eq sysSentry inactive

    sentryctl status test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: client_send_and_recv failed)' ${tmp_log}"

    sentryctl start test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: client_send_and_recv failed)' ${tmp_log}"

    sentryctl stop test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: client_send_and_recv failed)' ${tmp_log}"

    sentryctl reload test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: client_send_and_recv failed)' ${tmp_log}"

    sentryctl list 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: client_send_and_recv failed)' ${tmp_log}"

    # 启动sysSentry服务测试sentryctl命令执行成功场景
    systemctl start sysSentry.service
    pid1=$(ps -ef | grep syssentry | grep -v grep | awk '{print $2}')


    systemctl start sysSentry.service
    pid2=$(ps -ef | grep syssentry | grep -v grep | awk '{print $2}')

    expect_eq $pid1 $pid2

    sentryctl start stop test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: error: unrecognized arguments)' ${tmp_log}"

    sentryctl start test_sentryctl_exception aaa 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: error: unrecognized arguments)' ${tmp_log}"

    sentryctl aaa test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: error: argument cmd_type: invalid choice:)' ${tmp_log}"

    add_test_config "test_task" "pkill test_task" "period" 60 3600 "test_sentryctl_exception"
    expect_eq $? 0
    
    sentryctl reload test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(type of mod is different from old type, reload failed)' ${tmp_log}"

    sentryctl aaa test_sentryctl_exception 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(sentryctl: error: argument cmd_type: invalid choice:)' ${tmp_log}"

}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service
    systemctl stop xalarmd.socket xalarmd.service
    kill -9 `pgrep -w test_task`

    rm -rf ${tmp_log} test/sysSentry/test_task
    rm -rf /usr/bin/test_task /etc/sysSentry/tasks/test_sentryctl_exception.mod
}
set -x
run_testcase
