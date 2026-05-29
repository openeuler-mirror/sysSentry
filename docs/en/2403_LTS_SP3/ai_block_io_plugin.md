# AI Threshold-Based Slow Drive Detection Plugin

Users can perform slow drive fault detection through the AI threshold-based slow drive detection plugin. When the plugin detects a slow drive in the system, it reports the result to the `xalarmd` service, users can view alarm results by subscribing to alarms or running the `get_alarm` command. For details about alarm subscription and the `get_alarm` command, see [Installation and Usage](./installation_and_usage.md).

## Usage Restrictions

1. The AI threshold-based slow drive detection plugin can detect the following slow drives:
   - Slow drives due to I/O pressure: The alarm log contains keyword `io_press`.
   - Slow drives due to drive faults: The alarm log contains keyword `driver_slow`.
   - Slow drives due to I/O stack anomalies: The alarm log contains keyword `kernel_slow`.
   - Slow drives due to unknown reasons: The alarm log contains keyword `unknown`.
2. The performance overhead (including CPU utilization, memory usage, and I/O throughput) of the AI threshold-based slow drive detection plugin during operation does not exceed 5% of the total environment capacity.
3. Only the openEuler-20.03-LTS-SP4 version is supported, and the 4.19.90 kernel is used.
4. Slow drive detection can be performed for `nvme-ssd`, `sata-ssd`, and `sata-hdd` drives.
5. Slow drive detection can be performed only for `nvme-ssd`, `sata-ssd`, and `sata-hdd` drives. For details about how to identify drive types, see [FAQs - Question 2](./question_and_answer.md#question-2-how-do-i-identify-the-types-of-drives-in-the-system).
6. Before starting the average threshold inspection, ensure that the `sysSentry`, `xalarmd`, and `sentryCollector` services are running.

## Installing the Plugin

### Prerequisites

The `sysSentry` inspection plugin has been installed, and I/O-related collection items have been configured for the `sentryCollector` collection service. For details about how to configure the collection items, see [Installation and Usage](./installation_and_usage.md).

### Installing the Software Package

```shell
yum install -y ai_block_io pysentry_notify pysentry_collect python3-numpy
```

### Adding ai_block_io to the Framework for Management

```shell
[root@openEuler ~]# sentryctl reload ai_block_io
```

## Configuration File Description

The configuration file of the `ai_block_io` plugin is stored in `/etc/sysSentry/plugins/ai_block_io.ini`. Modifications to the configuration file take effect upon the next startup of the inspection task.

| Section             | Parameter               | Description                                                  | Default Value        | Mandatory or Not|
| ------------------- | --------------------- | ------------------------------------------------------------ | -------------- | ------ |
| [log]               | level                 | Log level. The value can be `debug`, `info`, `warning`, `error`, or `critical`. If the value is not configured or is invalid, the default value is used.| info           | Y      |
| [common]            | disk                  | Drive names, separated by commas (,). Value `default` indicates all drives in the current environment. If the configuration is incorrect, only the correct fields are retained.| default        | Y      |
| [common]            | stage                 | Monitoring stages, separated by commas (,). Currently, the following nine stages are supported: `throtl`, `wbt`, `gettag`, `plug`, `deadline`, `hctx`, `requeue`, `rq_driver`, and `bio`. The stages provided may vary depending on the drive type. Value `default` indicates all phases supported by the drive. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits. Note: The `bio` stage must be included in manual configurations.| default        | Y      |
| [common]            | iotype                | I/O types, separated by commas (,). Two types are supported: `read` and `write`. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| read,write     | Y      |
| [common]            | period_time           | Inspection interval of the plugin. The value is an integer, in seconds. The value must be an integer multiple of the `sentryCollector` collection interval and cannot exceed the value of `max_save` of `sentryCollector`. For details, see the `sentryCollector` configuration file.| 1              | Y      |
| [algorithm]         | train_data_duration   | Training data collection duration for the AI threshold algorithm, in hours. The value is a floating-point number. A larger value leads to more stable algorithm statistical results. The value ranges from 0 to 720. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 24             | Y      |
| [algorithm]         | train_update_duration | Threshold update interval of the AI threshold algorithm, in hours. The value is a floating-point number. A smaller value leads to faster threshold update and higher false positive rate. The value ranges from 0 to `train_data_duration`. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 2              | Y      |
| [algorithm]         | algorithm_type        | AI threshold algorithm type. The value is a string. The options are `boxplot` and `n_sigma`. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| boxplot        | Y      |
| [algorithm]         | boxplot_parameter     | Parameter of the `boxplot` algorithm. The value is a floating-point number. A larger value leads to lower sensitivity to exceptions. The value ranges from 0 to 10. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 1.5            | N      |
| [algorithm]         | n_sigma_parameter     | Parameter of the `n_sigma` algorithm. The value is a floating-point number. A larger value leads to lower sensitivity to exceptions. The value ranges from 0 to 10. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 3.0            | N      |
| [algorithm]         | win_type              | Detection window type of the AI threshold algorithm, which is applicable to different detection purposes. The value is a string. The options are `not_continuous`, `continuous`, and `median`. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| not_continuous | Y      |
| [algorithm]         | win_size              | Detection window length of the AI threshold algorithm. For slow I/O detection, only data within this window is checked for anomalies. The value is an integer ranging from 0 to 300. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 30             | Y      |
| [algorithm]         | win_threshold         | Number of threshold breaches for the AI threshold algorithm. The value is an integer. A smaller value results in faster anomaly reporting but a higher false positive rate. The value ranges from 1 to `win_size`. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 6              | Y      |
| [latency_DISK_TYPE] | read_avg_lim          | Upper limit of the average read latency, in us. This parameter indicates the limit on the average read I/O latency in the window. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| See note 3 below.             | Y      |
| [latency_DISK_TYPE] | write_avg_lim         | Upper limit of the average write latency, in us. This parameter indicates the limit on the average write I/O latency in the window. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| See note 3 below.             | Y      |
| [latency_DISK_TYPE] | read_tot_lim          | Absolute upper limit for read latency, in us. Any read latency above this upper limit is identified as anomalous. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| See note 3 below.             | Y      |
| [latency_DISK_TYPE] | write_tot_lim         | Absolute upper limit for write latency, in us. Any write latency above this upper limit is identified as anomalous. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| See note 3 below.             | Y      |
| [iodump]            | read_iodump_lim       | Absolute upper limit for read `iodump`. The value is an integer. Any read `iodump` count above this upper limit is identified as anomalous. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 0              | Y      |
| [iodump]            | write_iodump_lim      | Absolute upper limit for write `iodump`. The value is an integer. Any write `iodump` count above this upper limit is identified as anomalous. A larger value results in a higher false negative rate of the algorithm. If this parameter is not configured, the default configuration is used. If the configuration is incorrect, an error is reported and the process exits.| 0              | Y      |

>![](figures/icon-note.gif)**NOTE:**
>
>1. Adjusting any parameters in the `latency_[DISK_TYPE]` or `iodump` configuration section will affect the accuracy of the algorithm. A larger value results in a higher false negative rate, while a smaller value results in a higher false positive rate. You need to make choices based on experience.
>2. `[DISK_TYPE]` indicates the drive type. Currently, only three types are supported: `sata_ssd`, `nvme_ssd`, and `sata_hdd`.
>3. The default configuration of `read_avg_lim`, `write_avg_lim`, `read_tot_lim`, and `write_tot_lim` varies depending on the drive type. The details are as follows:
>    - For `sata_ssd` drives, the default values of `read_avg_lim`, `write_avg_lim`, `read_tot_lim`, and `write_tot_lim` are 10000 us, 10000 us, 50000 us, and 50000 μs, respectively.
>    - For `nvme_ssd` drives, the default values of `read_avg_lim`, `write_avg_lim`, `read_tot_lim`, and `write_tot_lim` are 300 us, 300 us, 500 us, and 500 us, respectively.
>    - For `sata_hdd` drives, the default values of `read_avg_lim`, `write_avg_lim`, `read_tot_lim`, and `write_tot_lim` are 15000 us, 15000 us, 50000 us, and 50000 us, respectively.
>4. When selecting a detection type for `win_type`, the options are as follows:
>    - `not_continuous`: Abnormal points in the detection window do not need to be consecutive.
>    - `continuous`: Abnormal points in the detection window must be consecutive.
>    - `median`: Checks whether the median of all data points in the detection window exceeds the threshold.
>5. If the algorithm type is `boxplot`, `boxplot_parameter` is mandatory. If the algorithm type is `n_sigma`, `n_sigma_parameter` is mandatory.

### Using the AI Threshold-Based Slow Drive Detection Plugin

1. Start an inspection.

   ```shell
   sentryctl start ai_block_io
   ```

2. Check the status of the inspection plugin.

   ```shell
   sentryctl status ai_block_io
   ```

   If the status is `RUNNING`, the plugin is running. If the status is `EXITED`, the plugin has exited.

3. View alarm information.

   ```shell
   sentryctl get_alarm ai_block_io -s 1 -d
   ```

   Example:

   ```shell
    [
        {
            "alarm_id": 1002,
            "alarm_type": "ALARM_TYPE_RECOVER",
            "alarm_level": "MINOR_ALARM",
            "timestamp": "2024-10-28 09:53:41",
            "alarm_info": {
                "alarm_source": "ai_block_io",
                "driver_name": "sda",
                "io_type": "write",
                "reason": "driver_slow",
                "block_stack": "bio,rq_driver",
                "alarm_type": "latency",
                "details": {
                    "latency": "{'read': {'bio': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 'rq_driver': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]}, 'write': {'bio': [17.8, 17.9, 18.0, 69.5, 79.2, 79.6, 80.4, 79.9, 81.2, 79.9, 76.2, 81.3, 78.7, 81.0, 81.0, 77.8, 79.1, 78.4, 82.1, 79.2, 80.1, 77.6, 79.5, 81.7, 78.4, 80.6, 77.5, 81.9, 81.1, 78.3], 'rq_driver': [15.1, 15.2, 15.3, 23.7, 28.8, 25.6, 27.0, 24.5, 28.2, 28.0, 26.0, 28.1, 27.3, 28.2, 28.7, 26.4, 26.8, 26.0, 28.1, 26.4, 27.5, 24.8, 27.7, 27.0, 25.7, 29.2, 25.6, 28.8, 27.9, 26.5]}}",
                    "iodump": "{'read': {'bio': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 'rq_driver': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]}, 'write': {'bio': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 'rq_driver': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]}}",
                }
            }
        }
    ]
   ```
   
   The fields in the output are described as follows:

   | Field| Description|
   | --- | --- |
   | alarm_id | ID of the alarm reported by the user. For the AI threshold-based slow drive detection plugin, the ID is fixed at `1002`.|
   | alarm_type | Alarm type. The alarm type of the AI threshold-based slow drive detection plugin is `ALARM_TYPE_OCCUR`, indicating that an alarm is generated.|
   | alarm_level | Alarm severity. The alarm severity of the AI threshold-based slow drive detection plugin is `MINOR_ALM`, indicating that the system is abnormal and has attempted to rectify the fault through automatic isolation.|
   | timestamp | Alarm report time.|
   | alarm_info | Alarm details, which is customized by the AI threshold-based slow drive detection plugin. The content reported by different inspection plugins is different.|

   The fields in `alarm_info` are described as follows:
   
   | Fields in alarm_info| Description|
   | --- | --- |
   | alarm_source | Alarm plugin name. For the AI threshold-based slow drive detection plugin, the value is fixed at `ai_block_io`.|
   | driver_name | Name of the drive for which the alarm is generated, for example, `sda`.|
   | io_type | I/O type of the alarm. The value can be:<br>1. `read`: slow drive fault alarm for read I/Os.<br>2. `write`: slow drive fault alarm for write I/Os.|
   | reason | Alarm cause. The AI threshold-based slow drive detection plugin may have the following four alarm causes:<br>1. `io_press`: slow drive alarm caused by high I/O pressure.<br>2. `driver_slow`: slow drive alarm caused by drive faults.<br>3. `kernel_slow`: slow drive alarm caused by I/O stack exceptions.<br>4. `unknown`: slow drive alarm caused by unknown faults.|
   | block_stack | Stage where the slow I/O exception occurs. This can be a combination of the following stages: `bio`, `throtl`, `wbt`, `rq_driver`, `gettag`, `plug`, `deadline`, `hctx`, and `requeue`. For detailed descriptions of these stages, see [Secondary Development Guide - Plugin Event Alarm Reporting](./developer_guide.md#reporting-plugin-event-alarms).|
   | details | Information recorded in the alarm log, which is displayed only when the `get_alarm` command is executed with the `-d` or `--detailed` option. The information includes the `latency` (I/O latency data) and `iodump` (number of I/Os that time out and are not completed) of the abnormal drive.|

4. Stop the inspection.

   ```shell
   sentryctl stop ai_block_io
   ```

5. View inspection results.
   
   After the inspection is stopped, you can view the inspection results.

   ```shell
   sentryctl get_result ai_block_io
   ```
