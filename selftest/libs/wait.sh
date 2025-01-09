#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/shopt.sh"

function wait_cmd_condition() {
    local cmd=$1
    local condition=$2
    local expect=$3
    local interval=$4
    local count=$5
    local debug=${6:-0}
    local i=0
    local ret=0
 
    shopt_set +e
    for ((i = 0; i < $count; i++)); do
        if [ "$debug" -eq 0 ]; then
            eval "$cmd" &> /dev/null
        else
            eval "$cmd"
        fi
        ret=$?
 
        local tmp_condition="[ $ret $condition $expect ]"
        if eval "$tmp_condition"; then
            return 0
        else
            sleep "$interval"
            ret=1
        fi
    done
    shopt_recover
    return $ret
}
 
function wait_cmd_ok() {
    local cmd=$1
    local interval=$2
    local count=$3
    local debug=${4:-0}
 
    wait_cmd_condition "$cmd" "=" "0" "$interval" "$count" "$debug"
    return $?
}
 
function wait_cmd_nok() {
    local cmd=$1
    local interval=$2
    local count=$3
    local debug=${4:-0}
 
    wait_cmd_condition "$cmd" "!=" "0" "$interval" "$count" "$debug"
    return $?
}
