# cpu_patrol

#### 介绍
Cpu_patrol is a module  used to check cpu, and it depends on the openEuler 5.10.

#### 软件架构
软件架构说明
cat-cli 命令行程序函数调用cpu_patrol动态库进行cpu巡检

#### 安装教程

1.  clone源码或下载源码压缩包解压
2.  进入catlib目录运行cmake命令`cmake  -B ./build/  -S .  -D CMAKE_INSTALL_PREFIX=/usr/local -D CMAKE_BUILD_TYPE=Release`
3.  编译及安装`cd build && make && make install`

#### 使用说明

详见命令`cat-cli -h`
