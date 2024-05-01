#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
source "libs/lib.sh"
source "test/common.sh"
set +e

tmp_log="tmp_log"

# 查看更新配置文件是否生效

function pre_test() {
    common_pre_test "test_task" "pkill test_task" "period" 60 3600 "test_sentryctl_reload"
}

function do_test() {

    sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(status: WAITING | RUNNING)' ${tmp_log}"

    sentryctl start test_sentryctl_reload
    expect_eq $? 0

    update_test_config "test_task" "pkill test_task" "period" 60 5
    cp mod/test_config.mod /etc/sysSentry/tasks/test_sentryctl_reload.mod

    sentryctl reload test_sentryctl_reload
    expect_eq $? 0

    sentryctl stop test_sentryctl_reload
    expect_eq $? 0

    sentryctl start test_sentryctl_reload
    expect_eq $? 0

    sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(status: RUNNING)' ${tmp_log}"

    sleep 7
    pid=$(ps aux | grep test_task | grep -v grep | awk '{print $2}')
    if [ -z "$pid" ]; then
        sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
        expect_true "grep -E '(status: FAILED)' ${tmp_log}"
    fi

    update_test_config "test_task" "pkill test_task" "period" 20 3600
    cp mod/test_config.mod /etc/sysSentry/tasks/test_sentryctl_reload.mod

    sentryctl reload test_sentryctl_reload
    expect_eq $? 0

    sentryctl stop test_sentryctl_reload
    expect_eq $? 0

    sentryctl start test_sentryctl_reload
    expect_eq $? 0

    sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(status: RUNNING)' ${tmp_log}"

    sleep 13

    sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(status: WAITING)' ${tmp_log}"

    sleep 9

    sentryctl status test_sentryctl_reload 2>&1 | tee ${tmp_log} | cat
    expect_true "grep -E '(status: RUNNING)' ${tmp_log}"
}

function post_test() {
	while [[ -n "`ps aux|grep -w syssentry|grep -v grep`" ]]; do
		kill -9 `pgrep -w syssentry`
		kill -9 `pgrep -w test_task`
		sleep 1
	done
   rm -rf ${tmp_log} test_task /usr/bin/test_task /etc/sysSentry/tasks/test_sentryctl_reload.mod
}
set -x
run_testcase
