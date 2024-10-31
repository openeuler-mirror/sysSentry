#!/bin/bash

# 检查是否输入了两个参数
if [ $# -ne 2 ]; then
    echo "使用方法: $0 <KERNEL_SOURCE> <NVME_INJECT_PATCH>"
    exit 1
fi

# 定义变量
KERNEL_SOURCE="$1"
NVME_INJECT_PATCH="$2"
KERNEL_RPM="kernel-source-4.19.90-2101.1.0.0055.oe1.aarch64.rpm"
KERNEL_SOURCE_PATH=$(dirname "$KERNEL_SOURCE")

# 保存初始目录
INITIAL_DIR=$(pwd)


#构建过程
echo "开始执行编译脚本..."

echo "步骤1: 检查编译工具链是否完整..."
# 需要检查的工具列表
tools=("gcc" "make" "libtool" "patch")

# 遍历工具列表，检查每个工具是否已安装
for tool in "${tools[@]}"; do
    if ! rpm -qa | grep -q "$tool"; then
        echo "工具 $tool 未安装，需要安装。"
        missing=true
    else
        echo "工具 $tool 已安装。"
    fi
done

# 如果有缺失的工具，提示用户需要安装
if [ "$missing" = true ]; then
    echo "检测到编译工具链不完整，请安装上述缺失的工具。"
else
    echo "编译工具链完整。"
fi

sleep 2;

# 步骤2: 切换到内核源码目录，若没有则创建目录
echo "步骤2: 切换到内核源码目录，若没有则创建目录..."
if [ ! -d "$KERNEL_SOURCE_PATH" ]; then
  mkdir -p "$KERNEL_SOURCE_PATH" || { echo "创建内核源码目录失败"; exit 1; }
fi
cd "$KERNEL_SOURCE_PATH" || { echo "切换目录失败"; exit 1; }

# 步骤3: 使用rpm2cpio和cpio提取内核源码包
echo "步骤3: 提取内核源码包..."
rpm2cpio "$KERNEL_RPM" | cpio -div || { echo "提取内核源码包失败"; exit 1; }

sleep 2;
 
# 步骤4: 复制补丁文件到内核源码目录
echo "步骤4: 复制补丁文件到内核源码目录..."
cp "$NVME_INJECT_PATCH" "$KERNEL_SOURCE_PATH/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host/" || { echo "复制补丁文件失败"; exit 1; }
cp "$KERNEL_SOURCE_PATH/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/include/linux/lightnvm.h" "/lib/modules/4.19.90-23.48.v2101.ky10.aarch64/build/include/linux/"
cp "$KERNEL_SOURCE_PATH/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/include/uapi/linux/lightnvm.h" "/lib/modules/4.19.90-23.48.v2101.ky10.aarch64/build/include/uapi/linux/"

# 步骤5: 切换到内核源码的drivers/nvme/host目录
echo "步骤5: 切换到内核源码的drivers/nvme/host目录..."
cd "$KERNEL_SOURCE_PATH/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host" || { echo "切换目录失败"; exit 1; }

# 步骤6: 应用补丁
echo "步骤6: 应用补丁..."
patch -p0 < "nvme_inject.patch" || { echo "应用补丁失败"; exit 1; }

sleep 2;

# 步骤7: 清理之前的编译文件
echo "步骤7: 清理之前的编译文件..."
make clean || { echo "清理编译文件失败"; exit 1; }

# 步骤8: 编译模块
echo "步骤8: 编译模块..."
make || { echo "编译模块失败"; exit 1; }

# 步骤9: 切换回初始目录并拷贝生成的 nvme.ko
echo "步骤9: 切换回初始目录并拷贝生成的 nvme.ko..."
cd "$INITIAL_DIR" || { echo "切换回初始目录失败"; exit 1; }
cp "$KERNEL_SOURCE_PATH/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host/nvme.ko" . || { echo "拷贝 nvme.ko 失败"; exit 1; }

echo "nvme.ko拷贝成功"


