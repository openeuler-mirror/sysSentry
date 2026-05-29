# Secondary Development Guide

As an inspection task management framework, sysSentry can manage both provided plugins and user-developed plugins. To develop a new plugin and manage it through the sysSentry framework, the following are needed:

1. Compiling the plugin management configuration file
2. Completing plugin function development

This document describes how to use the external interfaces provided by sysSentry to develop new plugins.

## Plugin Management Configuration File

New plugins developed by users can be managed using sysSentry. To do so, a corresponding configuration file must be added for the new plugin. This file should be placed in the `/etc/sysSentry/tasks/` directory and named `[Plugin_name].mod`. Assuming that the plugin name is `test`, refer to the following configuration example:

```shell
[root@openEuler ~]# cat /etc/sysSentry/tasks/test.mod
[common]
enabled=yes                         # Specifies whether to load the plugin. This parameter is mandatory.
task_start=/usr/bin/test            # Plugin startup command. This parameter is mandatory.
task_stop=pkill -f /usr/bin/test    # Plugin stop command. This parameter is mandatory.
type=oneshot                        # Plugin task type. This parameter is mandatory.
alarm_id=1100                       # Alarm ID. This parameter is optional.
alarm_clear_time=5                  # Alarm clearance time. This parameter is optional.
```

## Plugin Function Development

### Plugin Usage Restrictions

New plugins developed by users must meet the following requirements:

1. There is no restriction on the plugin development language. However, Python or C language is recommended. Currently, sysSentry offers secondary development interfaces exclusively for Python and C.
2. All plugins must be executable files.
3. All plugins must contain a stop command. Running the command can accurately stop the plugin task without affecting the running of other programs in the system.

### (Optional) Obtaining Collected Data

To use the sentryCollector collection service to obtain system data, see [Interconnecting with the sentryCollector Collection Service](#interconnecting-with-the-sentrycollector-collection-service).

### Reporting Plugin Event Alarms

You can use the alarm reporting interface to report plugin alarm information to the xalarmd service and use the get_alarm interface to view the alarm content.

```shell
[root@openEuler ~]# sentryctl get_alarm <Plugin_name>
```

sysSentry provides external interfaces for both Python and C languages.

#### Restrictions on Alarm Reporting

1. Alarm reporting only supports 128 alarm IDs ranging from 1001 to 1128. ID 1001 is reserved for memory inspection, and ID 1002 is reserved for slow I/O detection.
2. A maximum of 8191 characters are allowed for alarm reporting.

#### Reporting Plugin Alarms Using Python

Install the pysentry_notify software package.

```shell
[root@openEuler ~]# yum install -y pysentry_notify
```

`Interface` alarm information reporting

| Interface  | xalarm_report(alarm_id, alarm_level, alarm_type, puc_paras)  |
| ------ | ------------------------------------------------------------ |
| Description  | The inspection plugin can use this interface to report alarm information to the xalarmd service.             |
| Parameter  | `alarm_id`: alarm ID, which is an integer.<br>`alarm_level`: alarm severity. The value can be `MINOR_ALM` (minor alarm), `MAJOR_ALM` (major alarm), or `CRITICAL_ALM` (critical alarm).<br>`alarm_type`: alarm type. The value can be `ALARM_TYPE_OCCUR` (alarm generation) or `ALARM_TYPE_RECOVER` (fault recovery).<br>`punc_params`: alarm description, which is a character string.|
| Restrictions  | 1. The alarm ID ranges from 1001 to 1128. Currently, IDs 1001 (memory inspection) and 1002 (slow I/O detection) have been occupied. It is not advised to use them.<br>2. The alarm description can contain a maximum of 8191 characters. The communication is in JSON format during slow I/O inspection. For details about the fields in the JSON format, see "Description of the JSON format for reporting slow I/Os" below.|
| Return value| If an alarm is reported successfully, the return value is `True`. Otherwise, the return value is `False`.         |

Description of the JSON format for reporting slow I/Os:

| Parameter| Type        | Value                                                |
| ------------ | ---------------- | ------------------------------------------------------------ |
| device_name  | Character string          | Name of the faulty disk where a slow I/O event occurs, for example, `/dev/sda`.                  |
| reason       | Character string          | Cause of the slow I/O event. The value can be `disk_slow` (slow I/Os possibly caused by the disk response code), `kernel_stack` (slow I/Os possibly caused by the kernel stack), or `high_press` (slow I/Os possibly caused by heavy service loads).|
| block_stack  | Character string list      | List of abnormal I/O call stacks, for example, ["bio","rq_driver"]. The value can be `bio`, `throtl`, `wbt`, `gettag`, `plug`, `deadline`, `hctx`, `requeue`, or `rq_driver`.|
| io_type      | Character string          | Type of slow I/Os. The value can be `read` (slow read I/Os) or `write` (slow write I/Os).|
| alarm_source | Character string          | Alarm source plugin. The value can be `avg_block_io` (average threshold detection plugin alarm) or `ai_block_io` (AI threshold detection plugin alarm).|
| alarm_type   | Character string          | Type of the slow I/O alarms. The value can be `latency` (indicates that a slow I/O alarm is generated when the latency exceeds the threshold) or `iodump` (indicates that a slow I/O alarm is generated when the number of I/Os that are not completed due to timeout exceeds the threshold).|
| details      | Data list in JSON format| Detailed I/O latency data list|

Example:

```python
from xalarm.sentry_notify import (
    xalarm_report,
    MAJOR_ALM,
    ALARM_TYPE_OCCUR
)

ALARM_ID = 1002
ALARM_MSG = """
{
"alarm_info": {
            "alarm_source": "avg_block_io",
            "driver_name": "sda",
            "io_type": "write",
            "reason": "IO press",
            "block_stack": "bio,wbt",
            "alarm_type": "latency",
            "details": {
                "latency": "gettag: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], rq_driver: [0,0,0,0,0,437.9,0,0,0,0,517,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], bio: [0,0,0,0,0,521.1,0,0,0,0,557.8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], wbt: [0,0,0,0,0,0,8.5,0,0,0,0,12.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]",
                "iodump": "gettag: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], rq_driver: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], bio: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], wbt: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]"
            }
}
}
"""

if __name__ == "__main__":
    ret = xalarm_report(ALARM_ID, MAJOR_ALM, ALARM_TYPE_OCCUR, ALARM_MSG)
    if ret == -1:
        print("send failed.")
```

#### Reporting Plugin Alarms Using the C Language

Install the libxalarm software package.

```shell
[root@openEuler ~]# yum install -y libxalarm
```

The libxalarm-devel package (build dependency, not runtime dependency) also needs to be installed in the development environment.

```shell
[root@openEuler ~]# yum install -y libxalam-devel
```

`Interface` alarm information reporting

| Interface  | int xalarm_Report(unsigned short usAlarmId, unsigned char ucAlarmLevel, unsigned char ucAlarmType, char *pucParas); |
| ------ | ------------------------------------------------------------ |
| Description  | sysSentry alarm reporting interface, which is used to report alarms to be forwarded to the xalarmd service.    |
| Parameter  | `usAlarmId`: alarm ID<br>`usAlarmLevel`: alarm severity. The value can be `MINOR_ALM` (minor alarm), `MAJOR_ALM` (major alarm), or `CRITICAL_ALM` (critical alarm).<br>`ucAlarmType`: alarm type. The value can be `ALARM_TYPE_OCCUR` (alarm generation) or `ALARM_TYPE_RECOVER` (fault recovery).<br>`pucParas`: alarm description. The maximum length is 8191 characters.|
| Restrictions  | 1. The alarm ID ranges from 1001 to 1128. Currently, IDs 1001 (memory inspection) and 1002 (slow I/O detection) have been occupied. It is not advised to use them.<br>2. The alarm description can contain a maximum of 8191 characters.|
| Return value| If `0` is returned, the interface invoking is successful. If `-1` is returned, the interface invoking fails.|

Example:

```shell
[root@openEuler ~]# cat send_alarm.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>

#define ALARMID 1002

int main(int argc, char **argv)
{
    int alarmId = ALARMID;
    int level = MAJOR_ALM;
    int type = ALARM_TYPE_OCCUR;
    unsigned char *msg = "{\""
                            "alarm_info\": {"
                                "\"alarm_source\": \"avg_block_io\","
                                "\"driver_name\": \"sda\","
                                "\"io_type\": \"write\","
                                "\"reason\": \"IO press\","
                                "\"block_stack\": \"bio,wbt\","
                                "\"alarm_type\": \"latency\","
                                "\"details\": {"
                                    "\"latency\": \"gettag: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], rq_driver: [0,0,0,0,0,437.9,0,0,0,0,517,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], bio: [0,0,0,0,0,521.1,0,0,0,0,557.8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], wbt: [0,0,0,0,0,0,8.5,0,0,0,0,12.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\","
                                    "\"iodump\": \"gettag: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], rq_driver: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], bio: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], wbt: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\""
                                    "}"
                                "}"
                            "}\0";

    int ret = xalarm_Report(alarmId, level, type, msg);
    
    if (ret == -1) {
        printf("send failed.\n");
    }

    return 0;
}
[root@openEuler ~]# gcc send_alarm.c -o send_alarm -lxalarm
```

### Logging

The logs generated during the running of the sysSentry framework are stored in the `/var/log/sysSentry/sysSentry.log` file. You can view the logs to obtain the framework running details.

#### Location and Format of Plugin Logging

When developing an inspection plug-in, you are advised to save the logs generated during the running of the plugin in the `/var/log/sysSentry/` directory and name the log file as `[Plugin_name].log`. It is recommended that the log file name be the same as the plugin name.

When related information is recorded in logs, the recommended log format is as follows: `<Timestamp> - <Log_level> - [<File_name: line_number>] - <Log_message>`
The timestamp format is YYYY-MM-DD HH:MM:SS,FF, for example `2006-01-02 15:04:05.99`.

The log level can be `debug`, `info`, `warning`, `error`, or `critical`.
The meaning of each level is as follows:

- `debug`: lowest level, which is used to record detailed technical information to help with development and debugging.
- `info`: records normal running information about the program, such as the startup and shutdown.
- `warning`: records the situations that may cause problems but do not affect the running of the program.
- `error`: records critical problems that cause function failures or program interruption.
- `critical`: highest level, which is used to record very critical problems that may cause the program to stop running.

It is recommended that the log level be set to `info` when the program is running properly.

#### Plugin Log Rotation Configuration

The logs of the sysSentry framework and plugins are automatically rotated based on the `logrotate` mechanism. Log rotation is a system management technology used to manage the size and number of log files and prevent log files from occupying too much disk space.

Each time `logrotate` is triggered, the system checks the size of the log files of the sysSentry framework and plugins. If the size of a log file exceeds 4096 KB, `logrotate` is performed. The logs of a plugin or service can be rotated twice at most, and the rotated logs are compressed.

##### Example of Plugin Log Rotation Configuration

For details about how to configure log rotation, see the `/etc/logrotate.d/sysSentry` file.
The current configuration of `/etc/logrotate.d/sysSentry` is as follows:

```shell
/var/log/sysSentry/*.log{
    compress
    missingok
    notifempty
    copytruncate
    rotate 2
    size +4096k
    hourly
}
```

The meaning of each configuration item is as follows:

- `/var/log/sysSentry/*.log` indicates the log files to be dumped. The asterisk (*) is a wildcard, indicating all log files whose names end with `.log` in the `/var/log/sysSentry/` directory.
- `compress` indicates that log files are compressed by default to save memory space. You can also change it to `nocompress`, indicating that log files are not compressed. The default value `compress` is recommended.
- `missingok` indicates that `logrotate` does not report an error or stop processing other log files if a log file is missing. The default value of the configuration item is recommended.
- `notifempty` indicates that log files are not dumped if they are empty. The default value of the configuration item is recommended.
- `copytruncate` indicates that the current log file is copied and then cleared during log dump, and the copied file is compressed. This configuration item is usually used for log files that are being used by system processes to ensure that the processes can continue to write new logs. The default value of the configuration item is recommended.
- `rotate 2` indicates that two old log files are retained after the rotation operation. Once the number of old log files exceeds two, `logrotate` automatically deletes the old log files to release space for new log files. You can define the number of log files as required.
- `size +4096k` indicates that log dump is automatically triggered when the size of a log file exceeds 4096 KB. You can define the file size as required.
- `hourly` indicates that log rotation is performed every hour. The value can be `hourly`, `daily`, `weekly`, `monthly`, and others. In this example, `logrotate` rotates log files every hour. It is recommended that this item be configured to `hourly` by default.

##### Changing the Plugin Log Rotation Configuration

For new plugins:

1. You can modify the existing configuration items in `/etc/logrotate.d/sysSentry`. (Note: This operation will modify the configuration of all log files whose names end with `.log` in the `/var/log/sysSentry/` directory.)
2. To set different configurations for different log files in the `/var/log/sysSentry/` directory, configure each log file separately in `/etc/logrotate.d/sysSentry` as follows:

    ```shell
    /var/log/sysSentry/`[Log_name 1]`.log{
        # Required configuration items
    }
    /var/log/sysSentry/`[Log_name 2]`.log{
        # Required configuration items
    }
    ```

Do not set different configurations for the same log, as shown in the following example:

```shell
# A BAD EXAMPLE
/var/log/sysSentry/*.log{
    compress
}
/var/log/sysSentry/example.log{
    nocompress
}
```

In this example, the wildcard * represents all log files whose names end with `.log` in the `/var/log/sysSentry/` directory, including `example.log`. These files are configured to be compressed during log rotation. However, `example.log` is separately configured in the configuration file to not be compressed. This may cause unexpected behavior.

If the `/etc/logrotate.d/sysSentry` file is manually modified, you can run the `logrotate -f /etc/logrotate.d/sysSentry` command to manually trigger log rotation.
For details about how to use `logrotate`, visit <https://linux.die.net/man/8/logrotate>.

### Result Reporting

You can use the interface provided by sysSentry to report the plugin inspection result to the sysSentry service and run the `get_result` command to view the result.

```shell
[root@openEuler ~]# sentryctl get_result <Plugin_name>
```

External interfaces for Python and C languages are provided.

#### Restrictions on Result Reporting

1. The result reporting interface must be invoked before the inspection plugin stops running.
2. The inspection result should be reported only once in the lifecycle of an inspection plugin.
3. The inspection result should be reported regardless of whether the running of the inspection plugin succeeds or fails.

#### Python Inspection Result Reporting Interface

`Structure` ResultLevel

```python
class ResultLevel(Enum):
    """result level for report_result"""
    PASS = 0    # The inspection task is successfully executed and no exception occurs.
    FAIL = 1    # The inspection task is skipped or not executed due to reasons such as dependency missing or unsupported environments.
    SKIP = 2    # The inspection task fails to be executed due to reasons such as command execution errors.
    MINOR_ALM = 3   # The inspection task is complete. The system is abnormal, and the fault has been  rectified by means such as automatic isolation.
    MAJOR_ALM = 4   # The inspection task is complete. The system is abnormal and needs to be rectified by means such as restart.
    CRITICAL_ALM = 5    # The inspection task is complete. A critical alarm is generated. The system or hardware encounters an unrecoverable fault. You are advised to replace the system or hardware.
```

`Interface` result reporting

| Interface  | int report_result(task_name: str, result_level : ResultLevel, report_data : str) -> int |
| ------ | ------------------------------------------------------------ |
| Description  | Used by the module tool to report the inspection result to sysSentry.                         |
| Parameter  | `task_name`: inspection task name<br>`result_level`: exception level of the inspection result. For details about the optional parameters, see the `ResultLevel` structure.<br>`report_data`: detailed inspection result, which must be a character string converted from the JSON format.|
| Return value| Normal: `0`; abnormal: a value other than `0`|

Example:

```python
[root@openEuler ~]# python3
Python 3.7.9 (default, Dec 11 2023, 19:40:40) 
[GCC 7.3.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> import json
>>> from syssentry.result import ResultLevel, report_result
>>> report_result("test", ResultLevel.PASS, json.dumps({}))
0
```

#### C Language Inspection Result Reporting Interface

Install the libxlaram software package.

```shell
[root@openEuler ~]# yum install -y libxlaram
```

The libxalarm-devel package (build dependency, not runtime dependency) also needs to be installed in the development environment.

```shell
[root@openEuler ~]# yum install -y libxalam-devel
```

`Structure` RESULT_LEVEL <a id="resultlevel_c"></a>

```c
enum RESULT_LEVEL {
    RESULT_LEVEL_PASS = 0,  // The inspection task is complete and no exception occurs.
    RESULT_LEVEL_FAIL = 1,  // The inspection task is skipped or not executed due to reasons such as dependency missing or unsupported environments.
    RESULT_LEVEL_SKIP = 2,  // The inspection task fails to be executed due to reasons such as command execution errors.
    RESULT_LEVEL_MINOR_ALM = 3, // The inspection task is complete. The system is abnormal, and the fault has been  rectified by means such as automatic isolation.
    RESULT_LEVEL_MAJOR_ALM = 4, // The inspection task is complete. The system is abnormal and needs to be rectified by means such as restart.
    RESULT_LEVEL_CRITICAL_ALM = 5,  // The inspection task is complete. A critical alarm is generated. The system or hardware encounters an unrecoverable fault. You are advised to replace the system or hardware.
};
```

`Interface` result reporting

| Interface  | int report_result(const char \*task_name, enum RESULT_LEVEL result_level, const char \*report_data); |
| ------ | ------------------------------------------------------------ |
| Description  | Used by the module tool to report the inspection result to sysSentry.                         |
| Parameter  | `task_name`: inspection task name<br>`result_level`: exception level of the inspection result. For details about the optional parameters, see the `ResultLevel` structure.<br>`report_data`: detailed inspection result, which must be a character string converted from the JSON format.|
| Return value| Normal: `0`; abnormal: a value other than `0`|

Example:

```shell
[root@openEuler ~]# cat report_res.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>

int main(int argc, char *argv[]) {
    enum RESULT_LEVEL result_lv = 0;
    result_lv = RESULT_LEVEL_PASS;
    details = "{\"a\": 1, \"b\": 2}";
    int res = report_result(task_name, result_lv, details);
    if (res == -1) {
        printf("failed send result to sysSentry\n");
    }

    return 0;
}
[root@openEuler ~]# gcc report_res.c -o report_res -lxalarm
```

## Interconnecting with the sentryCollector Collection Service

The sysSentry software contains the sentryCollector service, which is used for data collection. Users can use the sentryCollector service to periodically collect system data.

### Restrictions on Data Collection

1. Currently, only system I/O data can be collected, and two solutions are provided: eBPF-based collection and kernel lock-free collection.
2. Only data of NVMe SSDs, SATA SSDs, and SATA HDDs can be collected.
3. By default, the sentryCollector service uses eBPF-based collection solution to collect I/O data. If you want to use the kernel lock-free collection solution, see [FAQs - Question 1](./question_and_answer.md#question-1-how-do-i-use-the-kernel-lock-free-collection-solution-to-collect-data).

### I/O Data Collection

The sentryCollector service can periodically collect data of specified disks using either of the following solution:

- Kernel lock-free collection: The kernel collects data and reports the collection result to the user mode for reading. The kernel needs to be recompiled.
- eBPF-based collection: Data is collected via eBPF-based kernel tracing without recompiling the kernel.

#### Differences Between Kernel Lock-free Collection and eBPF-based Collection

The differences between kernel lock-free collection and eBPF-based collection are as follows:

1. Different collection stages
    An I/O is divided into multiple stages from its occurrence to completion. The following table lists the stages supported by the two collection solutions.

    | Stage        | bio    | rq_driver | throtl   | wbt    | gettag | plug     | deadline | bpf      | requeue  | hctx     |
    | ------------ | ------ | --------- | -------- | ------ | ------ | -------- | -------- | -------- | -------- | -------- |
    | Kernel lock-free collection| Supported| Supported   | Supported  | Supported| Supported| Supported  | Supported  | Supported  | Supported  | Supported  |
    | eBPF-based collection    | Supported| Supported   | Not supported| Supported| Supported| Not supported| Not supported| Not supported| Not supported| Not supported|

2. Different types of I/Os that are collected

| I/O Type | Kernel Lock-free Collection| eBPF-based Collection| Data  |
| ------- | ------------ | -------- | ---------- |
| read    | Supported      | Supported  | Read I/O      |
| write   | Supported      | Supported  | Write I/O      |
| flush   | Supported      | Not supported| Flush I/O  |
| discard | Supported      | Not supported| Discard I/O|

Except for the preceding two differences, the data types and supported systems for the kernel lock-free collection are the same as those for the eBPF-based collection.

1. Both collection solutions support the 4.19 kernel and x86_64 and AArch64 architectures.
2. Both collection solutions can collect data of NVMe SSDs, SATA SSDs, and SATA HDDs.
3. Both collection solutions can collect `latency`, `iodump`, `iolength`, and `iops` data.
    - `latency`: indicates the latency data in a specified period.
    - `iodump`: indicates the number of I/Os that timed out and were not completed within the specified period. For example, an I/O is considered timed out if it remains incomplete after running for more than 1 second.
    - `iolength`: indicates the I/O queue length.
    - `iops`: indicates the number of I/Os completed per second.

#### External Interfaces

Currently, only Python interfaces are provided. You need to install the pysentry_collect software package.

```shell
[root@openEuler ~]# yum install -y pysentry_collect

```

`Interface 1` Viewing the disk type

| Interface  | get_disk_type(disk)                                          |
| ------ | ------------------------------------------------------------ |
| Description  | Queries the disk type from the collection module.                                    |
| Parameter  | `disk`: disk name, for example, `sda`. This parameter is mandatory.                            |
| Restrictions  | The disk name cannot exceed 32 characters.                                        |
| Return value| The return value format is {"ret": value1, "message":value2}.<br>`value1` can be `0` or a positive integer. The value `0` indicates success, and other values indicate failure.<br>`message` is a character string, indicating the disk type. If the value of `ret` is not 0, `message` is an empty string. The supported disk types are as follows:<br>"message": "0"  -- NVMe SSDs<br>"message": "1"  -- SATA SSDs<br>"message": "2"  -- SATA HDDs<br>Example of return values:<br>\- The disk type is not supported: {"ret": 8, "message": ""} # The value of ret is not 0.<br>- The function is executed successfully: {"ret": 0, "message": "1"}  # The disk is of the SATA SSD type.|

Example:

```shell
[root@openEuler ~]# python3
Python 3.7.9 (default, Dec 11 2023, 19:40:40) 
[GCC 7.3.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> from sentryCollector.collect_plugin import get_disk_type, Disk_Type
>>> res = get_disk_type("sda")
>>> res
{'ret': 0, 'message': '1'}
>>> curr_disk_type = int(res['message'])
>>> curr_disk_type
1
>>> Disk_Type[curr_disk_type]
'sata_ssd'
```

`Interface 2` Checking whether the collection is valid

| Interface  | is_iocollect_valid(period, disk_list=None, stage=None)       |
| ------ | ------------------------------------------------------------ |
| Description  | Checks whether the collection is within the specified range and whether the collection period is valid.                      |
| Parameter  | `period`: user-defined collection period, in seconds. The value is an integer. This parameter is mandatory.<br>`disk_list`: disk list. The default value is `None`, indicating that data of all disks are collected. This parameter is optional. The value can be a customized list, for example, ["sda", "sdb", "sdv"].<br>`stage`: collection stage. The default value is `None`, indicating that all collection stages are focused. This parameter is optional. The value can be a customized stage list, for example, ["wbt", "bio"].|
| Restrictions  | 1. The collection period ranges from 1 to 300.<br>2. The number of disks in the disk list cannot exceed 10. If the number of disks exceeds 10, data of the first 10 disks are collected by default. The disk name in the disk list cannot exceed 32 characters.<br>3. The number of collection stages cannot exceed 15, and the stage name cannot exceed 20 characters.|
| Return value| The return value format is {"ret": value1, "message":value2}.<br>`value1` can be `0` or a positive integer. The value `0` indicates success, and other values indicate failure.<br>The value of `message` is a character string, indicating the valid disks and the stages corresponding to the disks. If the character string is empty, all disks do not support data collection. The format is as follows:<br>{"disk_name1": ["stage1", "stage2"], "disk_name2": ["stage1", "stage2"], ...}  <br>Example of return values:<br>- Verification failed: {"ret": 1, "message": {}} #  The value of ret is not 0.<br>- Verification succeeded, and all disks do not support data collection: {"ret": 0, "message": {}}<br>- Some disks do not support data collection (the disks that support data collection and the corresponding stages are returned in `message`): {"ret": 0, "message": {"sda": ["bio", "gettag"], "sdb": ["bio", "gettag"]}}|

Example:

```shell
[root@openEuler ~]# python3
Python 3.7.9 (default, Dec 11 2023, 19:40:40) 
[GCC 7.3.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> from sentryCollector.collect_plugin import is_iocollect_valid
>>> is_iocollect_valid(1, ["sda"])
{'ret': 0, 'message': '{"sda": ["throtl", "wbt", "gettag", "plug", "deadline", "hctx", "requeue", "rq_driver", "bio"]}'}
```

`Interface 3` Querying specified data

| Interface  | get_io_data(period, disk_list, stage, iotype)                |
| ------ | ------------------------------------------------------------ |
| Function  | Checks whether the collection is within the specified range and whether the collection period is valid.                      |
| Parameter  | `period`: user-defined collection period, in seconds. The value is an integer. This parameter is mandatory.<br>`disk_list`: disk list, for example, ["sda", "sdb", "sdv"]. This parameter is mandatory.<br>`stage`: read stage, for example, ["bio", "gettag", "wbt"]. This parameter is mandatory.<br>`iotype`: I/O type, for example, ["read", "write"]. Only `read`, `write`, `flush`, and `discard` are supported. This parameter is mandatory.|
| Restrictions  | 1. The collection period ranges from 1 to 300 and is an integral multiple of the value of `period_time`. The multiple cannot exceed the value of `max_save`. For details about the two values, see /etc/sysSentry/sentryCollector.conf.<br>2. The number of disks in the disk list cannot exceed 10. If the number of disks exceeds 10, data of the first 10 disks are collected by default. The disk name in the disk list cannot exceed 32 characters.<br>3. The number of collection stages cannot exceed 15, and the stage name cannot exceed 20 characters.<br>4. The number of I/O types cannot exceed 4, and the number of characters cannot exceed 7 (the longest being `discard`).|
| Return value| The return value format is {"ret": value1, "message":value2}.<br>`value1` can be `0` or a positive integer. The value `0` indicates success, and other values indicate failure.<br>The value of `message` is a character string, indicating the data of the current period processed by the collection module. The format is as follows:<br>"{"disk_name1": {"stage1": {"read": [latency, iodump, iolength, iops],"write": [write_latency, write_iodump, iolength, iops]},"stage2": {…}},…}"  <br>Example of return values:<br>- Data fails to be obtained: {"ret": 1, "message": {}} # The value of ret is not 0.<br>- Data is obtained successfully: {"ret": 0, "message": "{"sda": {"bio": {"read": [0.1, 0, 100, 19], "write": [0.5, 3, 100, 12]}, "wbt": {}}, "sdb"…}"}<br>   [0.5, 3, 100, 12] corresponds to [latency (ns), number of I/O dumps, I/O queue length, IOPS].|

Example:

```shell
[root@openEuler ~]# python3
Python 3.7.9 (default, Dec 11 2023, 19:40:40) 
[GCC 7.3.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> from sentryCollector.collect_plugin import get_io_data
>>> get_io_data(1, ["sda"], ["bio"], ["read"])
{'ret': 0, 'message': '{"sda": {"bio": {"read": [0, 0, 0, 0]}}}'}
```
