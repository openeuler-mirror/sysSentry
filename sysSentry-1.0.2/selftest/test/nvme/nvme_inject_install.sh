#!/bin/bash

# 检查是否输入了两个参数
if [ $# -ne 1 ]; then
    echo "使用方法: $0 <NVME_KO>"
    exit 1
fi

# 定义变量
NVME_KO="$1"
MODULE_DEST="/lib/modules/4.19.90-23.48.v2101.ky10.aarch64/extra/hiodriver"

# 声明一个关联数组来存储设备路径和挂载点
declare -A device_paths
declare -A mount_points

# 安装过程

# 步骤1: 复制编译好的模块到模块目录，若没有则创建目录
echo "步骤1: 复制编译好的模块到模块目录，若没有则创建目录..."
if [ ! -d "$MODULE_DEST" ]; then
  mkdir -p "$MODULE_DEST" || { echo "创建模块目录失败"; exit 1; }
fi
cp "$NVME_KO" "$MODULE_DEST" || { echo "复制模块失败"; exit 1; }


# 步骤2: 卸载NVMe设备
echo "步骤2: 卸载NVMe设备..."
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
echo "NVMe 设备解挂载完成."

# 步骤3: 安装nvme_inject
echo "步骤3: 安装nvme_inject..."
if lsmod | grep -wo "crct10dif_neon";then
        echo "add_drivers+=\"crct10dif-neon\"" > /etc/dracut.conf.d/hiodriver.conf
else
        echo "add_drivers+=\"crct10dif-ce\"" > /etc/dracut.conf.d/hiodriver.conf
fi

if [ -e "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" ]; then
    depmod -aeF "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" "4.19.90-23.48.v2101.ky10.aarch64"  || :
fi

modules=( $(find /lib/modules/4.19.90-23.48.v2101.ky10.aarch64/extra/hiodriver | grep '\.ko$') )
if [ -x "/sbin/weak-modules" ]; then
    errinfo=$(printf '%%s\n' "${modules[@]}" | /sbin/weak-modules --add-modules 2>&1  | grep "No space left on device")
    if [ -n "$errinfo" ]; then
        echo "$errinfo"
        echo "Install kmod-hiodriver package failed."
        exit 1
    fi
fi

if [ "" != "kdump" ]; then
mods=( $(lsmod | grep -wo "nvme") )
if [ "${mods}" == "nvme" ]; then
    rmmod "nvme" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the old nvme kernel module."
        echo "Please uninstall the old nvme kernel module manually or reboot the system."
        exit 1
    else
        echo "Uninstall the old nvme kernel module successfully."
    fi
fi
mods=( $(lsmod | grep -wo "nvme_core") )
if [ "${mods}" == "nvme_core" ]; then
    rmmod "nvme_core" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the old nvme-core kernel module."
        exit 1
    else
        echo "Uninstall the nvme_core kernel module successfully."
    fi
fi

mods=( $(lsmod | grep -o "^nvme[ ]") )
if [ -z "${mods}" ]; then
    modprobe  "nvme" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to install the new nvme kernel module."
        echo "Please install the new nvme kernel module manually or reboot the system."
        exit 1
    else
        echo "Install the new nvme kernel module successfully."
        
        #servie name
        serv_name=$(systemctl -a -t service  | grep irq | grep balance | sed -e 's/[^ ]* //' | awk '{print $1}' | awk -F'.' '{print $1}')
        if [[ -z ${serv_name} ]]; then
            serv_name=$(chkconfig --list  | grep irq | grep balance | awk '{print $1}')
            if [[ -z ${serv_name} ]]; then
                serv_name=$(ls /etc/init.d/*irq*balance*  | awk -F'/' '{print $4}')
            fi
        fi

        if [[ -z ${serv_name} ]]; then
            echo Please start irqbalance service mannually.
        else
            #echo ${serv_name}
            serv_status=$(service ${serv_name} status  | grep running)
            #echo ${serv_status}
            if [[ -z ${serv_status} ]]; then
                echo Please start ${serv_name} service mannually..
            else
                sleep 10
                service ${serv_name} restart 
                if [[ $? -ne 0 ]]; then
                    echo Please start ${serv_name} service mannually...
                else
                    if [[ "4.19.90-23.48.v2101.ky10.aarch64" < "4.19" ]]; then
                        irqbalance -p 0 --hintpolicy=exact 
                    fi
                fi
            fi
        fi
    fi
fi

kdump=($(ls /boot/|grep kdump|grep init))
if [ -z "${kdump}" ];then
        echo ""
else
        echo "Modify kdump image.Please wait for a moment...50%%"
        echo "$kdump"
        mkdumprd -f /boot/$kdump  
fi
fi

echo "安装nvme驱动完成."


# 步骤4: 确认安装nvme_inject成功
echo "步骤4: 确认安装nvme_inject成功..."
result=$(find / -name finject 2>/dev/null)
if [ -z "$result" ]; then
    echo "失败：未找到finject 的文件，安装失败。"
    exit 1
else
    echo "成功：生成 finject 文件，安装成功。"
    echo "$result"
fi

# 步骤5: 重新挂载所有nvme设备
echo "步骤5: 重新挂载所有nvme设备..."
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

echo "nvme_inject安装执行完成。"


