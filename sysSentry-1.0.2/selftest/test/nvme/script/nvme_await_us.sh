#!/bin/bash

# 输入参数
continue=$1          # 是否连续
fault_time=$2        # 故障持续时间
interval=$3          # 故障间隔时间
delay=$4             # delay=150 时延超过正常时延150%; delay=250 时延超过正常时延250%; 
finject_path=$5      # find / -name finject找到finject的路径

# 计算故障周期长度和每小时循环的次数
fault_length=$((fault_time + interval))
fault_turn=$((60 / fault_length))

if [ "$continue" -eq 1 ]; then
    if [ "$delay" -eq 150 ]; then
        echo "注入连续慢盘故障, 延迟增加150%, 持续"$fault_time"s" 
        echo 0 1 200 > "$finject_path"
    elif [ "$delay" -eq 250 ]; then
        echo "注入连续慢盘故障, 延迟增加250%, 持续"$fault_time"s" 
        echo 0 1 400 > "$finject_path"
    elif [ "$delay" == "iodump" ]; then
        echo "注入磁盘IO HANG场景, 使得部分IO超时, 持续"$fault_time"s" 
        echo 0 1 2000000  > "$finject_path"
    fi
    sleep $fault_time
    echo 0 0 1 > "$finject_path"
else
    for i in $(seq 1 $fault_turn)
    do
        if [ "$delay" -eq 150 ]; then
            echo "注入间歇慢盘故障, 延迟增加150%" 
            echo 0 1 200 > "$finject_path"
        elif [ "$delay" -eq 250 ]; then
            echo "注入间歇慢盘故障, 延迟增加250%"
            echo 0 1 400 > "$finject_path"
        elif [ "$delay" == "iodump" ]; then
            echo "注入磁盘IO HANG场景, 使得部分IO超时, 持续"$fault_time"s" 
            echo 0 1 2000000 > "$finject_path"
        fi
        sleep $fault_time
        echo 0 0 1 > "$finject_path"
        sleep $interval
    done
fi

