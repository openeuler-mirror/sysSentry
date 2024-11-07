# nvme注错工具安装与使用
## 功能规格
nvme故障注入工具的原理是用工具源码编译出的nvme.ko来替换驱动的nvme.ko，工具源码通过宏定义HW_NVME_FAULT_INJECT在nvme驱动的一些接口（例如nvme_generate_tag）注入延时(usleep_range)。 
/sysSentry-1.0.2/selftest/test/nvme/nvme_inject.patch是nvme注入工具基于内核源码的补丁包。
/sysSentry-1.0.2/selftest/test/nvme/fault_inject/nvme_tools/kylin/scripts路径下是nvme注入工具的安装脚本。

## 关键约束
- CPU架构：ARM
- 操作系统：Kylin v10 （内核版本4.19.90-23.48.v2101.ky10.aarch64）
- 存储：NVMe SSD 仅支持单盘注入故障

## 安装方法
### step1.下载源码和patch补丁的使用
1.下载kernel-source源码包。kernel-source包的下载链接为https://repo.openeuler.org/openEuler-20.03-LTS/update/aarch64/Packages/
下载kernel-source-4.19.90-2101.1.0.0055.oe1.aarch64.rpm包并上传到测试环境中（例如保存路径为{path_to_kernel_source}）。

2.解压源码包。执行如下命令解压源码包，路径下出现/usr目录即为成功得到内核源码。
```bash
cd {path_to_kernel_source}
rpm2cpio kernel-source-4.19.90-2101.1.0.0055.oe1.aarch64.rpm | cpio -div
```

3.下载patch补丁包。下载openEuler sysSentry仓库中/sysSentry-1.0.2/selftest/test/nvme/路径下的nvme_inject.patch。

4.补丁包放入nvme驱动源码目录。将下载好的patch放入需要打补丁包的nvme驱动源码目录，目录为前面源码解压完路径下的/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host目录。
```bash
cp nvme_inject.patch {path_to_kernel_sorce}/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host
```

5.合入补丁。进入需要打补丁包的nvme驱动源码目录，并合入补丁更改源码。
```bash
cd {path_to_kernel_sorce}/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host 
patch -p0 < nvme_inject.patch
```
6.执行命令后出现以下结果，说明补丁已经合入源码
```bash
patching file ./Makefile
patching file ./adapter.c
patching file ./adapter.h
patching file ./core.c
patching file ./nvme.h
patching file ./pci.c
patching file ./trace.h
```

### step2. 编译nvme
1.生成nvme.ko文件
```bash
cd {path_to_kernel_sorce}/usr/src/linux-4.19.90-2101.1.0.0055.oe1.aarch64/drivers/nvme/host
make clean
make
```
如果生成了nvme.ko即为成功。出现以下输出也说明编译成功。
```bash
  LD [M]  /home/xu/test/kernel-4.19.90-2101.1.0/drivers/nvme/host/nvme.o
  Building modules, stage 2.
  MODPOST 1 modules
  CC      /home/xu/test/kernel-4.19.90-2101.1.0/drivers/nvme/host/nvme.mod.o
  LD [M]  /home/xu/test/kernel-4.19.90-2101.1.0/drivers/nvme/host/nvme.ko
make: Leaving directory '/usr/src/kernels/4.19.90-23.48.v2101.ky10.aarch64'
```

2.将nvme.ko复制到指定路径下。确认存在路径/lib/modules/[内核版本]/extra/hiodriver，如果/extra/hiodriver不存在，需要自行创建。
例如：
```bash
cp nvme.ko /lib/modules/4.19.90-23.48.v2101.ky10.aarch64/extra/hiodriver
```

### step3. 安装nvme工具
1.取消挂载nvme盘，例如
```bash
umount /dev/nvme0n1
```

2.下载并执行安装脚本。下载openEuler sysSentry仓库内容，进入下载后其中的/sysSentry-1.0.2/selftest/test/nvme/fault_inject/nvme_tools/kylin/scripts路径。执行install.sh脚本。
```bash
cd {patch_to_sysSentry}/sysSentry-1.0.2/selftest/test/nvme/fault_inject/nvme_tools/kylin/scripts
./install.sh
```
执行结果出现Install kmod-hiodriver package successfully.


### step4: 确认安装成功
1.执行如下命令，查找finject文件是否生成。
```bash
find / -name finject
```
若查询出现结果则安装成功。

2.cat {/path_to_finject}可以查看nvme工具状态信息。

3.重新挂载nvme盘即完成了nvme注错工具的完整安装过程。例如
```bash
mount /dev/nvme0n1 /data1
```

### step5: 卸载nvme工具
1.取消挂载nvme盘，例如
```bash
umount /dev/nvme0n1
```
如果umount失败，提示target is busy，可以通过 lsof /dev/nvme0n1查看盘上运行的进程，并通过 kill -9 [pid], killall -u [username], pkill [command] 等命令杀掉进程。

2.运行uninstall脚本并重启机器。
```bash
cd {patch_to_sysSentry}/sysSentry-1.0.2/selftest/test/nvme/fault_inject/nvme_tools/kylin/scripts
./uninstall.sh
```
最后reboot重启机器，完成卸载。

## 使用方法
### 故障注入和恢复命令
```bash
echo <type> <flag> [arg1 [arg2 [arg3…]]] > {/path_to_finject}
```
```bash
type 故障类型
flag 0-故障恢复 1-故障注入
arg 参数
path_to_finject 通过find / -name finject 执行结果获取
```

故障类型分为三种，nvme全局慢盘故障、nvme部分扇区慢盘故障、nvme只读故障。

1.nvme全局慢盘故障
```bash
type=0 
arg1为注入的延时时间
```
例如：
注入await故障（全局慢盘）400us echo 0 1 400 > {path_to_finject} 
注入await故障（全局慢盘）200us echo 0 1 200 > {path_to_finject}

恢复故障时将flag设置为0，例如
恢复await故障（全局慢盘）400us echo 0 0 400 > {path_to_finject} 


2.nvme部分扇区慢盘故障
```bash
type=2 
arg1为start_block，arg2为end_block。
工具会对这两个逻辑块号之间的逻辑扇区注入慢盘故障
```
例如：
注入UNC故障（部分慢盘，对于1~500逻辑块号之间的扇区注入）400us 
先设定注入的逻辑扇区 echo 2 1 1 500 > {path_to_finject}  
再注入400us延时 echo 0 1 400 > {path_to_finject}


恢复故障时将flag设置为0，例如
恢复UNC故障（部分慢盘） 
先取消注入延时echo 0 0 400 > {path_to_finject} 
再取消注入的逻辑扇区设定 echo 2 0 1 500 > {path_to_finject} 

3.nvme只读故障
```bash
type=3 
没有额外参数
```
例如：
注入只读故障 echo 3 1 > {path_to_finject} 

恢复故障时将flag设置为0，例如
恢复只读故障 echo 3 0 > {path_to_finject} 

