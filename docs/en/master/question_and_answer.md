# FAQs

## **Question 1: How Do I Use the Kernel Lock-Free Collection Solution to Collect Data?**

Kernel lock-free collection and eBPF-based collection cannot be used simultaneously, and kernel lock-free collection takes precedence over eBPF-based collection. The `sentryCollector` service uses eBPF-based collection by default on the openEuler-20.03-LTS-SP4 OS. To enable kernel lock-free collection, you must recompile the kernel using the following method:

Step 1: Download the `kernel-source` package and decompress it.

```shell
[root@openEuler ~]# yumdownloader kernel-source-4.19.90-2409.3.0.0294.oe2003sp4
# Replace [$ARCH] with x86_64 or aarch64 based on the machine architecture.
[root@openEuler ~]# rpm2cpio kernel-source-4.19.90-2409.3.0.0294.oe2003sp4.[$ARCH].rpm |cpio -div
[root@openEuler ~]# cd usr/src/linux-4.19.90-2409.3.0.0294.oe2003sp4.[$ARCH]/
```

Step 2: Perform the configuration and build operations.

```shell
[root@openEuler ~]# yum install -y openssl-devel bc rsync gcc gcc-c++ flex bison m4 ncurses-devel elfutils-libelf-devel

# Search for BLK_IO_HIERARCHY_STATS, enable all corresponding config options, then save and exit.
[root@openEuler ~]# make menuconfig

Modify the EXTRAVERSION = .blk_io in the Makefile (this change will be reflected in the final built kernel version string).

# Compile
[root@openEuler ~]# make -j 200 && make modules_install && make install
```

Step 3: Reboot the system using the new kernel.

```shell
[root@openEuler ~]# grubby --info ALL 	#Check the index of the newly compiled kernel.
[root@openEuler ~]# grubby --set-default-index N 	#Set N to the index of the newly compiled kernel.
[root@openEuler ~]# reboot
```

Step 4: Restart services related to `sysSentry`.

```shell
[root@openEuler ~]# systemctl restart xalarmd
[root@openEuler ~]# systemctl restart sysSentry
[root@openEuler ~]# systemctl restart sentryCollector
```

Check whether the following directory exists on the environment (replace `[disk]` with a valid drive name):

```shell
[root@openEuler ~]# ll /sys/kernel/debug/block/[disk]/blk_io_hierarchy/
```

If the directory exists, the `sentryCollector` service uses kernel lock-free collection.

## **Question 2: How Do I Identify the Types of Drives in the System?**

You can check all drive types and their `rotate` status in the current environment using the `lsblk -d` command:
1. NVMe SSD: The drive name starts with `nvme` and `rotate` is `0`.
2. SATA SSD: The drive name starts with `sd` and `rotate` is `1`.
3. SATA HDD: The drive name starts with `sd` and `rotate` is `0`.

## **Question 3: What System Data Does the Average Threshold-Based or AI Threshold-Based Slow Drive Detection Plugin Analyze During Runtime?**
The average threshold-based or AI threshold-based slow drive detection plugin obtains the `latency` and `iodump` data of specified drives at each stage through the `sentryCollector` service and perform data analysis:
- `latency`: I/O latency data, representing the time information of completed IOs within a statistical period.
- `iodump`: The count of I/Os that have timed out (not completed). If an I/O remains incomplete for more than 1 second, the `iodump` count increases by one.
  
SentryCollector supports both eBPF-based collection and kernel lock-free collection. By default, eBPF-based collection is used. For details about the differences between the two collection methods and how to use kernel lock-free collection, see [Secondary Development Guide](./developer_guide.md).

## **Question 4: Why Does the Status Remain EXITED After the sentryctl start avg_block_io/ai_block_io Command Is Executed?**
This may be caused by the following reasons: 

1. The `sentryCollector` service is not started: Check whether the service is running using the `systemctl status sentryCollector` command.
2. Invalid configuration fields: Check the `/var/log/sysSentry/avg_block_io.log` (or `ai_block_io.log`) file for error messages, and modify the `/etc/sysSentry/plugins/avg_block_io.ini` (or `ai_block_io.ini`) file based on the error content. Notably, if the `period_time` configuration does not match the `sentryCollector` configuration, the error message for `avg_block_io` will be `Cannot get valid disk` instead of a `period_time` error.
