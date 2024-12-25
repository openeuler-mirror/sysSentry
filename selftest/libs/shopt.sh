#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

 
syssentry_shopt=""
 
function shopt_set() {
    local new_opt=$1
 
    if [ -z "$syssentry_shopt" ]; then
        syssentry_shopt="$-"
    fi
 
    set "$new_opt"
}
 
function shopt_recover() {
    set +$-
    set -${syssentry_shopt}
    unset syssentry_shopt
}
