#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

 
ets_shopt=""
 
function shopt_set() {
    local new_opt=$1
 
    if [ -z "$ets_shopt" ]; then
        ets_shopt="$-"
    fi
 
    set "$new_opt"
}
 
function shopt_recover() {
    set +$-
    set -${ets_shopt}
    unset ets_shopt
}
