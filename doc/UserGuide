# sysSentry 介绍
sysSentry是一款故障巡检框架，为用户提供在后台进行故障巡检的能力；sysSentry通过提前发现系统中的软硬件故障并及时通知系统运维人员处理的方式，达到减少故障演变为现网事故、提升系统可靠性的目标。

## 一、安装sysSentry
```bash
yum install -y sysSentry xalarm libxalarm
```

## 二、快速使用指南

1、启动巡检框架
```bash
systemctl start sysSentry
systemctl start xalarmd

### 执行成功后可通过status命令查看状态为running
systemctl status sysSentry
systemctl status xalarmd
```

2、使用sentryctl命令管理框架
```bash
### 启动指定巡检任务
sentryctl start <module_name>
### 终止指定巡检任务
sentryctl stop <module_name>
### 列出所有已加载的巡检任务及当前状态
sentryctl list
### 查询指定巡检任务状态
sentryctl status <module_name>
### 重载指定任务配置
sentryctl reload <module_name>
```