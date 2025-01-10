#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

# 新命名风格断言
function log_info() {
    echo "[$(date +"%F %T")] [  INFO ] $*"
}

function log_warn() {
    echo -e "\033[33m[$(date +"%F %T")] [WARNING] $* \033[0m"
}

function log_error() {
    echo -e "\033[31m[$(date +"%F %T")] [ ERROR ] $* \033[0m"
}
