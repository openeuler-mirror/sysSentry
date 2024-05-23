#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

function common_pre_test() {

    kill -9 `ps aux|grep syssentry|grep -v grep|awk '{print $2}'`
    kill -9 `ps aux|grep test_task|grep -v grep|awk '{print $2}'`

    task_start="$1"
    task_stop="$2"
    type="$3"
    interval="$4"
    heartbeat_interval="$5"
    test_task_name="$6"

    update_test_config "$task_start" "$task_stop" "$type" $interval $heartbeat_interval

    gcc mod/test_task.c -o test_task
    cp test_task /usr/bin
    cp mod/test_config.mod /etc/sysSentry/tasks/"$test_task_name".mod

    syssentry &
    sleep 1
}

function update_test_config() {
      config_file="mod/test_config.mod"
      task_start="$1"
      task_stop="$2"
      type="$3"
      interval="$4"
      heartbeat_interval="$5"
 
      if [ -f "$config_file" ]; then
        sed -i "s/^task_start=.*/task_start=$task_start/" "$config_file"
        sed -i "s/^task_stop=.*/task_stop=$task_stop/" "$config_file"
        sed -i "s/^type=.*/type=$type/" "$config_file"
        sed -i "s/^interval=.*/interval=$interval/" "$config_file"
        sed -i "s/^heartbeat_interval=.*/heartbeat_interval=$heartbeat_interval/" "$config_file"
        echo "test_config.mod file has been updated."
      else
        echo "test_config.mod file not found."
      fi
}

function add_test_config() {
    task_start="$1"
    task_stop="$2"
    type="$3"
    interval="$4"
    heartbeat_interval="$5"
    config_file="/etc/sysSentry/tasks/$6"

    touch "$config_file"

    if [ -f "$config_file" ]; then
      echo "[common]" > "$config_file"
      echo "enabled=yes" >> "$config_file"
      echo "task_start=$task_start">> "$config_file"
      echo "task_stop=$task_stop">> "$config_file"
      echo "type=$type">> "$config_file"
      echo "interval=$interval">> "$config_file"
      echo "heartbeat_interval=$heartbeat_interval">> "$config_file"
      echo "test_config.mod file has been added."
    else
      echo "test_config.mod file not added."
    fi

    gcc mod/test_task.c -o test_task
    cp test_task /usr/bin

    syssentry &
    sleep 1
}

function look_pid() {
    task_task="$1"
    pid=$(ps -ef | grep '$task_task' | grep -v grep | awk '{print $2}')
    if [ -z "$pid"]
    then
        echo "task_task is not existed"
        return 0
    else
        echo "task_task is existed"
        return 1
    fi
}