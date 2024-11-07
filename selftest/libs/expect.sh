#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/log.sh"

ets_expects_failed=0
unexp_cmd="((++ets_expects_failed))"
 
function get_file_line() {
    echo "$(basename "${BASH_SOURCE[2]}")": ${BASH_LINENO[1]}
}
 
function get_expects_failed_num() {
    echo "${ets_expects_failed}"
}
 
function clean_expects_failed_num() {
    ets_expects_failed=0
}
 
function add_failure() {
    local msg=${1:-}
 
    ((++ets_expects_failed))
    log_error "add_failure(msg=${msg}) - $(get_file_line)"
    return 1
}
 
function expect_eq() {
    local actual=${1:-1}
    local expect=${2:-0}
    local msg=${3:-}
    if [ "${actual}" -eq "${expect}" ]; then
        return 0
    else
        ((++ets_expects_failed))
        echo "expect_eq(${actual}, ${expect}, msg=${msg}) - $(get_file_line)"
        return 1
    fi
}
 
function expect_ne() {
    local actual=${1:-1}
    local expect=${2:-1}
    local msg=${3:-}
 
    if [ "${actual}" -ne "${expect}" ]; then
        return 0
    else
        ((++ets_expects_failed))
        log_error "expect_ne(${actual}, ${expect}, msg=${msg}) - $(get_file_line)"
        return 1
    fi
}
 
function expect_gt() {
    local actual=${1:-0}
    local expect=${2:-1}
    local msg=${3:-}
 
    if [ "${actual}" -gt "${expect}" ]; then
        return 0
    else
        ((++ets_expects_failed))
        log_error "expect_gt(${actual}, ${expect}, msg=${msg}) - $(get_file_line)"
        return 1
    fi
}

function expect_true() {
    local cmd=$1
    local msg=$2
 
    if eval "${cmd}" &> /dev/null; then
        return 0
    else
        ((++ets_expects_failed))
        log_error "expect_true(${cmd}, msg=${msg}) - $(get_file_line)"
        return 1
    fi
}

function expect_false() {
    local cmd=$1
    local msg=$2
 
    if eval "${cmd}" &> /dev/null; then
        ((++ets_expects_failed))
        log_error "expect_false(${cmd}, msg=${msg}) - $(get_file_line)"
        return 1
    else
        return 0
    fi
}