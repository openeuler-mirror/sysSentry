#!/bin/bash

# 设置默认的 fio 配置文件
FIO_CONFIG="normal.fio"

# 检查是否传入了配置文件作为参数
if [ "$#" -ge 1 ]; then
    FIO_CONFIG=$1
fi

# 运行 fio
fio $FIO_CONFIG

echo "Fio test completed with configuration from $FIO_CONFIG."

