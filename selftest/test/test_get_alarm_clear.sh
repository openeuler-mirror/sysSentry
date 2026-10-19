#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.
# Test get_alarm cache eviction semantics:
#  - get_alarm persists eviction of alarms older than alarm_clear_time to the
#    cache (so a query reclaims expired entries, not only return filtering).
#  - time_range is a return-only query view: a narrow time_range must NOT
#    evict alarms still within clear_time. Otherwise a later wider query
#    would lose alarms the user expected to see.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/wait.sh"
set +e

mod_name="test_get_alarm_clear"
alarm_id=1002
alarm_clear_time=10
alarm_msg='{"alarm_source":"test_get_alarm_clear","details":{"key":"value"}}'
config_file="/etc/sysSentry/tasks/${mod_name}.mod"
inspect_conf="/etc/sysSentry/inspect.conf"
sysSentry_log="/var/log/sysSentry/sysSentry.log"

function pre_test() {
    kill -9 `ps aux|grep syssentry|grep -v grep|awk '{print $2}'` 2>/dev/null

    rm -rf ./checklog test/xalarm/send_demo
    gcc test/xalarm/send_demo.c -o test/xalarm/send_demo -lxalarm

    if [ -f "$config_file" ]; then
        rm -rf "$config_file"
    fi
    touch "$config_file"
    echo "[common]" > "$config_file"
    echo "enabled=yes" >> "$config_file"
    echo "task_start=/bin/true" >> "$config_file"
    echo "task_stop=/bin/true" >> "$config_file"
    echo "type=oneshot" >> "$config_file"
    echo "alarm_id=${alarm_id}" >> "$config_file"
    echo "alarm_clear_time=${alarm_clear_time}" >> "$config_file"

    cp "$inspect_conf" "${inspect_conf}.bak"
    sed -i 's/^level=.*/level=debug/' "$inspect_conf"

    echo > "$sysSentry_log"

    systemctl start xalarmd.socket xalarmd.service
    sleep 1
    systemctl start sysSentry.socket sysSentry.service
    sleep 3
}

function do_test() {
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    ./test/xalarm/send_demo ${alarm_id} 1 1 "${alarm_msg}" >> checklog 2>&1
    sleep 2

    # age ~2 < clear_time(10): alarm cached and visible.
    expect_true "sentryctl get_alarm ${mod_name} -s 10 | grep -q 'test_get_alarm_clear'" \
        "alarm visible before clear_time"

    sleep 4
    # age ~6 lands in (time_range=5, clear_time=10): a narrow -s 5 query
    # filters the alarm out of its response (6 > 5).
    expect_false "sentryctl get_alarm ${mod_name} -s 5 | grep -q 'test_get_alarm_clear'" \
        "narrow time_range filters alarm out of response"

    # age ~6 still < clear_time(10) and < time_range(20): the narrow -s 5 query
    # must NOT have evicted it, so a wider -s 20 query still sees it. If the
    # narrow query had persisted its slice, the alarm would be gone here.
    expect_true "sentryctl get_alarm ${mod_name} -s 20 | grep -q 'test_get_alarm_clear'" \
        "narrow time_range did not evict alarm from cache"

    sleep 8
    # age ~14 > clear_time(10): get_alarm evicts from cache. time_range(30) >
    # age(14), so return filtering alone would still return it; empty proves
    # cache-level eviction by the clear_time path.
    expect_false "sentryctl get_alarm ${mod_name} -s 30 | grep -q 'test_get_alarm_clear'" \
        "alarm evicted from cache after clear_time"
}

function post_test() {
    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    if [ -f "${inspect_conf}.bak" ]; then
        mv "${inspect_conf}.bak" "$inspect_conf"
    fi
    rm -rf ./checklog test/xalarm/send_demo "$config_file"
}

run_testcase
