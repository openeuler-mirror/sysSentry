#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source "libs/expect.sh"
OLD_PATH=$(pwd)
 
function run_testcase() {
    local testcasename=$1
    trap post_test EXIT INT TERM
    pre_test
    do_test
 
    if [ "$(get_expects_failed_num)" != "0" ]; then
        echo "$testcasename check failed!!"
	exit 1
    else
        echo "$testcasename  check successful!!"
	exit 0
    fi
}

