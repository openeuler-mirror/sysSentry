#!/bin/bash
# Copyright (c), 2026, Huawei Tech. Co., Ltd.
#
# It exercises the real sysSentry service's conflict=kill path against real
# processes. check_conflict() must:
#   - kill only processes whose /proc/<pid>/cmdline argv equals
#     shlex.split(task_start) exactly (same command + same args, no more, no
#     less);
#   - leave alone processes whose cmdline merely contains task_start as a
#     substring, or that run with different / extra arguments;
#   - when two tasks share an identical task_start, starting the second kills
#     the conflict left by the first.
#
# `pgrep -afx "<cmd> <args>"` performs the same full-cmdline exact match the
# Python cmdline_matches() relies on, so it is used to assert which processes
# survive / die.

source "libs/expect.sh"
source "libs/lib.sh"
source "libs/common.sh"
set +e

tmp_log="tmp_log"

# test program (long-running, accepts ignored argv so args show up in cmdline)
test_bin="test_task_with_argv"
test_src="test/sysSentry/${test_bin}.c"

# Several inspection tasks, including two with an identical task_start and
# others with different arguments / a bare generic word.
mod_same="conflict_kill_same"        # task_start with args: ... --tag same
mod_same2="conflict_kill_same2"      # identical task_start to mod_same
mod_diff="conflict_kill_diff"        # different args:        ... --tag diff
mod_short="conflict_kill_short"      # bare generic word task_start (no path/args)

all_mods=("$mod_same" "$mod_same2" "$mod_diff" "$mod_short")

start_with_args="/usr/bin/${test_bin} --tag same"
start_diff_args="/usr/bin/${test_bin} --tag diff"
start_short="${test_bin}"

function rm_mod_files() {
    for mod in "${all_mods[@]}"; do
        rm -rf "/etc/sysSentry/tasks/${mod}.mod"
    done
}

# Write a single oneshot task config with conflict=kill and task_stop=kill $pid
# (the $pid sentinel is substituted by sysSentry; "kill $pid" is whitelisted so
# each task's stop only kills its own child and never cross-interferes).
function write_conflict_config() {
    local mod_name="$1"
    local task_start="$2"
    local config_file="/etc/sysSentry/tasks/${mod_name}.mod"

    touch "$config_file"
    echo "[common]" > "$config_file"
    echo "enabled=yes" >> "$config_file"
    echo "task_start=${task_start}" >> "$config_file"
    echo "task_stop=kill \$pid" >> "$config_file"
    echo "type=oneshot" >> "$config_file"
    echo "conflict=kill" >> "$config_file"
}

function pre_test() {
    kill -9 `ps aux | grep syssentry | grep -v grep | awk '{print $2}'` 2>/dev/null
    kill -9 `ps aux | grep ${test_bin} | grep -v grep | awk '{print $2}'` 2>/dev/null

    rm_mod_files

    gcc "${test_src}" -o "test/sysSentry/${test_bin}"
    cp "test/sysSentry/${test_bin}" "/usr/bin/${test_bin}"

    write_conflict_config "$mod_same" "$start_with_args"
    write_conflict_config "$mod_same2" "$start_with_args"
    write_conflict_config "$mod_diff" "$start_diff_args"
    write_conflict_config "$mod_short" "$start_short"

    systemctl start xalarmd.socket xalarmd.service
    systemctl start sysSentry.socket sysSentry.service
    sleep 2
}

# Number of processes whose full cmdline equals <argv> exactly. Mirrors the
# Python cmdline_matches() rule (no substring, no extra args).
function count_exact() {
    pgrep -afx "$1" 2>/dev/null | wc -l
}

# True (0) if pid is still alive.
function pid_alive() {
    kill -0 "$1" 2>/dev/null
}

function do_test() {
    expect_service_status_eq sysSentry active
    expect_service_status_eq xalarmd active

    # --- Scenario 1: identical argv already running is killed on start ------
    # Pre-launch a process with the exact task_start argv of mod_same.
    $start_with_args &
    same_pid=$!
    sleep 1
    expect_eq "$(count_exact "$start_with_args")" 1 "pre-launch left one exact-argv process"
    pid_alive "$same_pid"
    expect_eq $? 0 "pre-launch process is alive"

    sentryctl start "$mod_same"
    expect_eq $? 0
    sleep 1
    expect_task_status_eq "$mod_same" "RUNNING"

    # The manually launched conflict must have been killed (exact argv match);
    # but the task itself spawns a child with the identical argv, so exactly
    # one such process must remain (the task's own child).
    expect_false "pid_alive $same_pid" "manual conflict process was killed"
    expect_eq "$(count_exact "$start_with_args")" 1 "exactly one exact-argv process remains (task child)"

    # --- Scenario 2: different argv is NOT killed (cross-task exact match) --
    # Launch a process with a different argv (--tag diff) and start mod_diff.
    # mod_diff's check_conflict kills its identical-argv conflict (the manual
    # one), while mod_same's still-running --tag same child must survive.
    $start_diff_args &
    diff_pid=$!
    sleep 1
    same_child_pid=$(pgrep -afx "$start_with_args" 2>/dev/null | head -1)

    sentryctl start "$mod_diff"
    expect_eq $? 0
    sleep 1
    expect_task_status_eq "$mod_diff" "RUNNING"

    expect_false "pid_alive $diff_pid" "different-argv manual process was killed (exact match on its own argv)"
    expect_true "pid_alive $same_child_pid" "mod_same child with different argv was NOT killed"
    # one --tag same (mod_same) + one --tag diff (mod_diff child)
    expect_eq "$(count_exact "$start_with_args")" 1 "mod_same child still alive"
    expect_eq "$(count_exact "$start_diff_args")" 1 "mod_diff child running"

    # --- Scenario 3: substring / extra args must NOT be killed ------------
    # task_start of mod_short is the bare word "test_task_with_argv". Under the
    # old substring code, ANY process whose cmdline contained that word
    # (including "test_task_with_argv --tag same/diff") would be killed. With
    # exact-argv matching, only a process with the bare single-token argv
    # matches, so the extra-args processes must all survive.
    "$test_bin" --tag extra &
    extra_pid=$!
    sleep 1

    sentryctl start "$mod_short"
    expect_eq $? 0
    sleep 1
    expect_task_status_eq "$mod_short" "RUNNING"

    # The substring-bearing processes (extra args) must survive: exact argv
    # never matches a cmdline that merely contains task_start.
    expect_true "pid_alive $extra_pid" "extra-args process survived (no substring kill)"
    expect_true "pid_alive $same_child_pid" "mod_same --tag same survived substring check"
    expect_eq "$(count_exact "$start_with_args")" 1 "mod_same child still alive after mod_short start"

    # --- Scenario 4: two tasks sharing an identical task_start -------------
    # Start mod_same2, whose task_start is identical to mod_same. Its
    # check_conflict must kill the --tag same process left by mod_same, then
    # start its own, leaving exactly one --tag same process overall.
    sentryctl start "$mod_same2"
    expect_eq $? 0
    sleep 1
    expect_task_status_eq "$mod_same2" "RUNNING"

    expect_eq "$(count_exact "$start_with_args")" 1 "single --tag same process after identical-argv task start"
    expect_eq "$(count_exact "$start_diff_args")" 1 "mod_diff child unaffected by same2"
    expect_true "pid_alive $extra_pid" "extra-args process still alive"

    # --- cleanup: stop tasks (kill $pid stops only each task's own child) ---
    for mod in "${all_mods[@]}"; do
        sentryctl stop "$mod" >/dev/null 2>&1
    done
    sleep 2

    # After stopping, no exact-argv conflict processes remain.
    expect_eq "$(count_exact "$start_with_args")" 0 "no --tag same process after stop"
    expect_eq "$(count_exact "$start_diff_args")" 0 "no --tag diff process after stop"
    # the extra-args process was never owned by a task; kill it explicitly
    pid_alive "$extra_pid" && kill -9 "$extra_pid" 2>/dev/null
}

function post_test() {
    for mod in "${all_mods[@]}"; do
        sentryctl stop "$mod" >/dev/null 2>&1
    done

    systemctl stop sysSentry.socket sysSentry.service 2>/dev/null
    systemctl stop xalarmd.socket xalarmd.service 2>/dev/null

    kill -9 `pgrep -x "${test_bin}"` 2>/dev/null
    rm -rf "${tmp_log}" "test/sysSentry/${test_bin}" "/usr/bin/${test_bin}"
    rm_mod_files
}

run_testcase
