#!/bin/bash

# 声明一个关联数组来存储设备路径和挂载点
declare -A device_paths
declare -A mount_points

# 卸载过程

# 步骤1: 卸载NVMe设备
echo "步骤1: 卸载NVMe设备..."
# 获取所有 NVMe 设备列表
nvme_devices=$(lsblk -d -o NAME,PATH | grep -i nvme)
# 检查是否获取到 NVMe 设备列表
if [ -z "$nvme_devices" ]; then
    echo "未找到 NVMe 设备。"
    exit 0
fi
# 遍历 NVMe 设备列表
while IFS=' ' read -r device path; do
    # 获取设备的挂载点
    mount_point=$(mount | grep "$path" | awk '{print $3}')
    # 检查设备是否已挂载
    if [ -n "$mount_point" ]; then
        echo "设备 $device ($path) 已挂载在 $mount_point，正在尝试解挂载..."
        # 解挂载设备
        if umount "$mount_point"; then
            echo "设备 $device ($path) 成功解挂载。"
            # 记录设备路径和挂载点
            device_paths[$device]="$path"
            mount_points[$device]="$mount_point"
        else
            echo "设备 $device ($path) 解挂载失败，请手动检查。"
            exit 1
        fi
    else
        echo "设备 $device ($path) 未挂载，跳过。"
    fi
done <<< "$nvme_devices"
echo "NVMe 设备解挂载完成。"


# 步骤2: 卸载nvme_inject
echo "步骤2: 卸载nvme_inject..."
if [ -e "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" ]; then
    depmod -aeF "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" "4.19.90-23.48.v2101.ky10.aarch64" > /dev/null || :
else
    depmod "4.19.90-23.48.v2101.ky10.aarch64" > /dev/null || :
fi

modules=( $(cat /var/run/rpm-kmod-hiodriver-modules 2> /dev/null) )
rm -f /var/run/rpm-kmod-hiodriver-modules 2> /dev/null
if [ -x "/sbin/weak-modules" ]; then
    errinfo=$(printf '%%s\n' "${modules[@]}" | /sbin/weak-modules --remove-modules 2>&1 >/dev/null | grep "No space left on device")
    if [ -n "$errinfo" ]; then
        echo "$errinfo"
        echo "Uninstall kmod-hiodriver package failed."
        exit 0
    fi
fi

if [ $1 -ne 0 ]; then
    echo "Uninstall kmod-hiodriver package successfully."
    exit 0
fi

if [ "" != "kdump" ]; then
mods=( $(lsmod | grep -wo "nvme") )
if [ "${mods}" == "nvme" ]; then
    rmmod "nvme" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the installed nvme kernel module."
        echo "Please uninstall the installed nvme kernel module manually or reboot the system."
    else
        echo "Uninstall the installed nvme kernel module successfully."
    fi
fi

standard_nvme=( $(modinfo -n nvme 2>/dev/null) )
if [ -n "${standard_nvme}" ]; then
    mods=( $(lsmod | grep -wo "nvme") )
    if [ -z "${mods}" ]; then
        modprobe  "nvme"  >/dev/null 2>&1
        if [ "$?" -ne 0 ]; then
            echo "Warning: fail to install the in-kernel nvme kernel module."
            echo "Please install the in-kernel nvme kernel module manually or reboot the system."
        else
            echo "Install the in-kernel nvme kernel module successfully."
                    
            #service name
            serv_name=$(systemctl -a -t service 2>/dev/nul | grep irq | grep balance | sed -e 's/[^ ]* //' | awk '{print $1}' | awk -F'.' '{print $1}')
            if [[ -z ${serv_name} ]]; then
                serv_name=$(chkconfig --list 2>/dev/nul | grep irq | grep balance | awk '{print $1}')
                if [[ -z ${serv_name} ]]; then
                    serv_name=$(ls /etc/init.d/*irq*balance* 2>/dev/nul | awk -F'/' '{print $4}')
                fi
            fi

            if [[ -z ${serv_name} ]]; then
                echo Please start irqbalance service manually.
            else
                #echo ${serv_name}
                serv_status=$(service ${serv_name} status 2>/dev/nul | grep running)
                #echo ${serv_status}
                if [[ -z ${serv_status} ]]; then
                    echo Please start ${serv_name} service manually..
                else
                    service ${serv_name} restart 2>/dev/nul
                    if [[ $? -ne 0 ]]; then
                        echo Please start ${serv_name} service manually...
                    else
                        if [[ "4.19.90-23.48.v2101.ky10.aarch64" < "4.19" ]]; then
                            irqbalance -p 0 --hintpolicy=exact &>/dev/nul
                        fi
                    fi
                fi
            fi
        fi
    fi
fi
fi

echo "Uninstall kmod-hiodriver package successfully."


sleep 5;
# 步骤3: 重新挂载所有nvme设备
echo "步骤3: 重新挂载所有nvme设备..."
for device in "${!mount_points[@]}"; do
    path=${device_paths[$device]}
    mount_point=${mount_points[$device]}
    echo "正在重新挂载设备 $device ($path) 到 $mount_point..."
    if mount "$path" "$mount_point"; then
        echo "设备 $device ($path) 成功重新挂载到 $mount_point。"
    else
        echo "设备 $device ($path) 重新挂载到 $mount_point 失败。"
        exit 1
    fi
done
echo "NVMe重新挂载完成。"

echo "nvme_inject卸载执行成功。"

