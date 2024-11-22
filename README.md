# sysSentry 介绍
sysSentry是一款故障巡检框架，为用户提供在后台进行故障巡检的能力；sysSentry通过提前发现系统中的软硬件故障并及时通知系统运维人员处理的方式，达到减少故障演变为现网事故、提升系统可靠性的目标。

## 一、普通用户安装sysSentry
```bash
yum install -y sysSentry pyxalarm 
```

## 二、开发者安装
### step1. 安装构建依赖
```bash
yum install -y cmake gcc-c++ make python3 python3-setuptools json-c json-c-devel elfutils-devel clang libbpf-devel llvm kernel-source kernel-devel libbpf
```

### step2. 下载源码
```bash
git clone https://gitee.com/openeuler/sysSentry.git
```

### step3. 编译sysSentry
```bash
cd sysSentry
make && make install
```

### step4. 编译插件（可选）
```bash
cd sysSentry
make [插件名] && make install-[插件名]
```

### step4. 启动服务
```bash
make startup
```

### step5. 删除软件
```bash
make clean
```

## 三、快速使用指南

1、启动巡检框架
```bash
systemctl start xalarmd
systemctl start sysSentry
systemctl start sentryCollector

### 执行成功后可通过status命令查看状态为running
systemctl status xalarmd
systemctl status sysSentry
systemctl status sentryCollector
```

2、使用sentryctl命令管理框架
```bash
# 启动指定巡检任务
sentryctl start <module_name>
# 终止指定巡检任务
sentryctl stop <module_name>
# 列出所有已加载的巡检任务及当前状态
sentryctl list
# 查询指定巡检任务状态
sentryctl status <module_name>
# 重载指定任务配置
sentryctl reload <module_name>
# 查看运行结果
sentryctl get_result <module_name>
# 查看告警信息
sentryctl get_alarm <module_name>
```

详细使用说明请参考[openEuler使用文档](docs.openeuler.org)。