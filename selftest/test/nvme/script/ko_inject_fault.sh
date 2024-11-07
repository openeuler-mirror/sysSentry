#!/bin/bash

# 输入参数
fault_type=$1        # 故障类型
continue=$2          # 是否连续
fault_time=$3        # 故障持续时间
interval=$4          # 故障间隔时间
delay=$5             # delay=150 时延超过正常时延150%; delay=iodump Hang住IO 1.5s

# 定义故障注入的路径
loop_path="num_4"

# 计算故障周期长度和每小时循环的次数
fault_length=$((fault_time + interval))
fault_turn=$((60 / fault_length))

# 初始化注入的故障类型列表
loop_cfe=""

# 判断是否为连续模式
if [ "$continue" -eq 1 ]; then
    loop_cfe="$fault_type"
else
    for i in $(seq 1 $fault_turn)
    do
        loop_cfe="$loop_cfe $fault_type"
    done
fi

# 待注入故障的设备和挂载点
export dev_name="nvme0n1"
export mount_path="/data1"

# 复制故障注入文件到指定目录
cp -f inject_fault/* $loop_path/

# 挂载设备
mount /dev/${dev_name} $mount_path

# 遍历 loop_path 中的各项，注入故障并执行脚本
for num_path in $loop_path
do
    # 复制 inject_fault 中的内容
    cp -f inject_fault/* $loop_path/
    
    for cfe in $loop_cfe
    do
        # 执行故障注入脚本
        # sh run_slow_disk.sh $dev_name $num_path $cfe
        mkdir -p $num_path/fio_slow_disk_${num_path}_${cfe}
        rm -f $num_path/fio_slow_disk_${num_path}_${cfe}/* 

        #sh $num_path/$cfe.sh $delay

        if [ "$delay" -eq 150] || ["$delay" -eq 250 ]; then
            sh ${num_path}/ko_${cfe}.sh $delay
        elif [ "$delay" == "iodump" ]; then
            echo "*****************************************开始插入ko ..... *******************************"
            insmod /root/kernel_kprobe/ko_${cfe}/test_${cfe}.ko max_times=50000 sleep_time=1500
        else
            echo "无效的 delay 参数: $delay"
            continue  # 如果 delay 参数无效，跳过此次循环
        fi

        dmesg -C
        dmesg |head -20

        # 故障持续时间
        sleep $fault_time

        # 根据 fault_type 动态生成模块名称
        module_name="test_${fault_type}"

        # 检查并卸载与 fault_type 相关的模块
        if lsmod | grep -q $module_name; then
            rmmod $module_name
            echo "$module_name module removed"
        else
            echo "$module_name module not loaded"
        fi

        # 故障间隔时间
        sleep $interval
    done
done


