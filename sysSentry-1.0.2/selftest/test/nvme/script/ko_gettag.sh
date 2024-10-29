#!/bin/bash

delay=$1

echo "*****************************************开始插入ko ..... *******************************"

if [ "$delay" -eq 150 ]; then
    insmod /root/kernel_kprobe/ko_gettag/test_gettag.ko max_times=50000000 sleep_time=30
else if [ "$delay" -eq 250 ]; then
    insmod /root/kernel_kprobe/ko_gettag/test_gettag.ko max_times=50000000 sleep_time=60
fi

dmesg -C
dmesg |head -20



# 需要等到fio进程结束之后再执行，否则可能造成系统卡死
#rmmod test_io

