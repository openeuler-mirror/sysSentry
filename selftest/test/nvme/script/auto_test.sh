#!/bin/bash

# 定义要注入的故障类型
fault_types=("hctx" "requeue" "deadline" "plug" "gettag" "single_disk" "full_disk")
# 定义持续时间和间隔时间
durations=(2 4 6)
intervals=(5 10)
# 定义delay值
delays=(150 250 "iodump")

# 故障连续模式 (continue=1,0)
continue_modes=(0 1)
continue_durations=(2 60)

# 循环故障类型
for fault_type in "${fault_types[@]}"; do
    for continue_mode in "${continue_modes[@]}"; do

        # 如果 fault_type 是 single_disk 或 full_disk，跳过 delay=iodump
        if [[ ("$fault_type" == "single_disk" || "$fault_type" == "full_disk") && "$continue_mode" -eq 1 ]]; then
            delays=(150 250)  # 盘异常场景：不包括持续的fail stop
        else
            delays=(150 250 "iodump")
        fi

        if [ "$continue_mode" -eq 1 ]; then
            # continue=1 时，持续60秒，不设置间隔
            interval=0
            for duration in "${continue_durations[@]}"; do
                for delay in "${delays[@]}"; do
                    if [[ "$delay" == "iodump" && "$duration" -eq 2 ]]; then
                        continue
                    fi
                    echo "开始注入 $fault_type 故障,continue=$continue_mode, fault_time=$duration, delay=$delay"
                    sh ko_inject_fault.sh "$fault_type" "$continue_mode" "$duration" "$interval" "$delay"
                done
            done
        else
            # continue=0 时，使用 2, 4, 6 秒的持续时间和 5 秒/10 秒间隔
            for duration in "${durations[@]}"; do
                for interval in "${intervals[@]}"; do
                    for delay in "${delays[@]}"; do
                        echo "开始注入 $fault_type 故障,continue=$continue_mode, fault_time=$duration, interval=$interval, delay=$delay"
                        sh ko_inject_fault.sh "$fault_type" "$continue_mode" "$duration" "$interval" "$delay"
                    done
                done
            done
        fi
    done
done

