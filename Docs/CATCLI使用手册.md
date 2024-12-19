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

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
