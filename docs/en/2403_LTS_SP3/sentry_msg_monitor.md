# UBPRM

This plugin supports the reception and reporting of event messages, including out-of-band power-off, OOM, `panic`, in-band power-off, and UnifiedBus memory faults, depending on the `xalarmd` service. The overall workflow is as follows: The kernel notification chain blocks the event process and sends a message. This message is forwarded through the kernel device, the user-space plugin, and `xalarmd` to the corresponding handler. Once processing is complete, the handler propagates the message back to the kernel. The kernel then determines the next operation based on the timing and result of the returned message.

## Hardware Specifications

- Only the AArch64 architecture is supported.
- Only servers that comply with the UnifiedBus protocol are supported.
- When UVB communication is used, the BIOS must support the corresponding functions. Otherwise, the UVB communication function will be unavailable.

## Usage Restrictions

- Currently, only single-process subscription and processing are supported.
- Ensure the kernel version is 6.6 or higher for full functionality. The following drivers must be included, or the plugin will fail to function:
  - `sentry_reporter`
  - `sentry_remote_reporter`

- The out-of-band power-off feature applies only to the power-off scenario on the BMC interface.
- Do not uninstall the `sentry_remote_reporter` or `sentry_reporter` drivers while an event transmission is pending.
- If the remote end fails to respond after an event occurs, the actual timeout for blocking the notification chain may exceed the user-defined time. This is a normal phenomenon caused by kernel timer inaccuracies.
- The CNA and EID information of the local node, as well as the information of other nodes in the cluster, must be configured via the management plane. If the information is not configured, hijacking will not be triggered when the system experiences a `panic` or in-band power-off.
- Link re-establishment is supported if an anomaly occurs after a successful URMA connection.
- The proper functioning of UnifiedBus communication links depends on correct configuration.
- Before a heartbeat response is received, `panic` or in-band power-off messages cannot be sent. If the link disconnects between two heartbeat detection cycles, it cannot be automatically re-established, and the `panic` or in-band power-off messages will fail to send.
- To enable `panic` event hijacking, add `crash_kexec_post_notifiers` to the kernel command-line parameters.
- To enable OOM event hijacking, add `numa_remote` to the kernel command-line parameters.
- UnifiedBus fault events are reported only to services registered for the `1013` event type. This fault type does not support retransmission.
- Detailed information reported for UnifiedBus fault events includes only the borrowed physical memory address and `memid`. The memory borrowing relationship must be retrieved by querying the memory borrowing management service within the system.
- UnifiedBus fault event reporting does not support fault storm suppression. Since the BIOS already provides suppression functions for fault events, `sysSentry` does not perform further suppression processing.
- When memory borrowing or sharing is performed in `Device` mode, if a `UCE` fault occurs, the kernel does not support isolating the faulty page. The service must subscribe to the corresponding fault event, unmap the memory, isolate the faulty page, and then remap the memory.
- The plugin sends a `SIGBUS` signal to the service thread using the memory address where the `UCE` fault occurred. The service thread must implement a custom interception and handling workflow for the `SIGBUS` signal. Otherwise, the service thread will be terminated according to the default signal behavior.

## Key Specifications

- A maximum of 16 nodes are supported in a cluster.
- When communication is based on URMA, the node EID must be configured in IPv6 format only.
- The hijacking features for out-of-band power-off, OOM, UB memory faults, `panic`, and in-band power-off flows are disabled by default. These features take effect only after being enabled through commands on the management plane.
- For `panic` and in-band power-off flow hijacking, the system sends an event message and blocks for a period before continuing the original flow. Related logs are recorded in `dmesg` (kernel log persistence is provided by the `kbox` service). The `panic` or in-band power-off flow will not be eventually blocked.
- Because the `panic` and in-band power-off flows are intercepted, the system reboot time for these scenarios increases compared with the original behavior. The delay equals the configured timeout interval, which is 35 seconds by default. If the node receives a response within the timeout interval, the blocking ends early and the original flow resumes.
- When UVB communication is used, if the CNA is invalid, the operation fails due to timeout. Since the BIOS timeout interval is 1 second, the BIOS can forward the next CNA only after 1 second.
- `sysSentry` supports processing and forwarding memory fault events reported by the UBUS driver. The forwarded events are custom fault types defined by `sysSentry`.
- `sysSentry` supports selecting whether to reset a process based on user configuration in scenarios where asynchronous access to remote memory fails. If a reset is required: In NUMA scenarios, it queries the corresponding process PID by physical address. In FD scenarios, it relies on an interface provided by the `obmm` component to query the process PID. Upon successfully obtaining the process `pid`, it sends a `SIGBUS` signal to trigger the process reset.

## Installing the Plugin

### Prerequisites

The `sysSentry` framework and `xalarmd` service have been installed.

### Installing the Software Package

```shell
yum install -y sentry_msg_monitor
```

### Adding sentry_msg_monitor to the Framework for Management

```shell
sentryctl reload sentry_msg_monitor
```

## Using UBPRM

### Enabling and Configuring Events

#### Configuring Out-of-Band Power-Off Events

Out-of-band power-off events are disabled by default. To enable or disable them, run the following command:

```shell
sentryctl set sentry_reporter --power_off=on/off
```

#### Configuring OOM Events

OOM events are disabled by default. To enable or disable them, run the following command:

```shell
sentryctl set sentry_reporter --oom=on/off
```

#### Configuring UnifiedBus Memory Fault Reporting Events

UnifiedBus memory fault reporting events are disabled by default. To enable or disable them, run the following command:

```shell
sentryctl set sentry_reporter --ub_mem_fault=on/off
```

You can configure whether to send a `SIGBUS` signal to the thread that uses the faulty memory in case of a UnifiedBus memory fault. This function is enabled by default. To enable or disable the function, run the following command:

```shell
sentryctl set sentry_reporter --ub_mem_fault_with_kill=on/off
```

#### Configuring the Panic Event Reporting Function

The `panic` events are cross-node and require sending messages to the primary node via URMA or UVB. Therefore, at least one of step 2 and step 3 below must be configured.

**Step 1** Set the EID and CNA information, and set the timeout interval as required.

```shell
sentryctl set sentry_remote_reporter --eid=eid --cna=cna --panic=on --panic_timeout_ms=xxx
```

Parameters for configuring `panic` events:

| Parameter| Value Range| Description|
| --- | --- | --- |
|--cna | 0 to 16777215| Configures CNA information of a node. The CNA must be unique.|
| --eid | IPv6 address| Configures EID information of a node. The EID is a 128-bit hexadecimal number. Every 16 bits (2 bytes) are separated by a semicolon (;). The length is fixed at 39 characters. You can configure one or two EIDs for a node, separated by semicolons (;).|
| --panic | `on` or `off`| Enables or disables the `panic` event hijacking function. The `panic` event hijacking function is disabled by default.|
| --panic_timeout_ms | 0 to 3600000|Configures notification chain blocking time, in milliseconds. The default value is `35000`.|

**Step 2 (Optional)** Initialize the parameters of the URMA communication component.

```shell
sentryctl set sentry_urma_comm --server_eid="eid11,eid21,...,eidn1;eid12,eid22,...,eidn2" --client_jetty_id=xxx
```

Initialization parameters of the URMA communication component:

| Parameter| Value Range| Description|
| --- | ---| --- |
|--server_eid |IPv6 address|Configures the EID information of other nodes to be connected to the current node. Links can be established for up to 32 nodes across 2 DIEs. When configuring EIDs for link establishment, the first EID is the local EID, and the following EIDs are the peer EIDs to be connected. EIDs are separated by commas (,). When configuring the link mode of multiple DIEs, use semicolons (;) to separate them.<br>  The sequence of local EIDs in `--server_eid` must be the same as those in `--eid`. Otherwise, the link establishment will fail.|
|--client_jetty_id |3 to 1023| Configures the `jetty id` information for the current node. An unoccupied value must be set.|

**Step 3 (Optional)** Initialize the parameters of the UVB communication component.

```shell
sentryctl set sentry_uvb_comm --server_cna="cna1; cna2;...;cnan"
```

Initialization parameters of the UVB communication component:

| Parameter| Value Range| Description|
| --- | ---| --- |
|--server_cna|0 to 16777215 (0xFFFFFF)| Configures the CNA information of the peer nodes to be connected to the current node. Use semicolons (;) to separate CNAs.<br>  - CNA information of multiple nodes is separated by semicolons (;).<br>  - You are advised not to set duplicate CNAs.|

#### Configuring the In-Band Power-Off Event Reporting Function

The `kernel reboot` events are cross-node and require sending messages to the primary node via URMA or UVB. Therefore, at least one of step 2 and step 3 below must be configured.

**Step 1** Set the EID and CNA information, and set the timeout interval as required.

```shell
sentryctl set sentry_remote_reporter --eid=eid --cna=cna --kernel_reboot=on --kernel_reboot_timeout_ms=xxx
```

Parameters for configuring `reboot` events:

| Parameter| Value| Description|
| --- | --- | --- |
|--cna | 0 to 16777215| Configures CNA information of a node. The CNA must be unique.|
| --eid | IPv6 address| Configures EID information of a node. The EID is a 128-bit hexadecimal number. Every 16 bits (2 bytes) are separated by a semicolon (;). The length is fixed at 39 characters. You can configure one or two EIDs for a node, separated by semicolons (;).|
| --kernel_reboot | `on` or `off`| Enables or disables the in-band `reboot` event hijacking function. The in-band power-off event hijacking function is disabled by default.|
| --kernel_reboot_timeout_ms | 0 to 3600000|Configures notification chain blocking time, in milliseconds. The default value is `35000`.|

**Step 2 (Optional)** Initialize the parameters of the URMA communication component.

```shell
sentryctl set sentry_urma_comm --server_eid="eid11,eid21,...,eidn1;eid12,eid22,...,eidn2" --client_jetty_id=xxx
```

Initialization parameters of the URMA communication component:

| Parameter| Value| Description|
| --- | ---| --- |
|--server_eid |IPv6 address|Configures the EID information of other nodes to be connected to the current node. Links can be established for up to 32 nodes across 2 DIEs. When configuring EIDs for link establishment, the first EID is the local EID, and the following EIDs are the peer EIDs to be connected. EIDs are separated by commas (,). When configuring the link mode of multiple DIEs, use semicolons (;) to separate them.<br>  The sequence of local EIDs in `--server_eid` must be the same as those in `--eid`. Otherwise, the link establishment will fail.|
|--client_jetty_id |3 to 1023| Configures the `jetty id` information for the current node. An unoccupied value must be set.|

**Step 3 (Optional)** Initialize the parameters of the UVB communication component.

```shell
sentryctl set sentry_uvb_comm --server_cna="cna1; cna2;...;cnan"
```

Initialization parameters of the UVB communication component:

| Parameter| Value Range| Description|
| --- | ---| --- |
|--server_cna |0 to 16777215 (0xFFFFFF)| Configures the CNA information of the peer nodes to be connected to the current node. Use semicolons (;) to separate CNAs.<br>  - CNA information of multiple nodes is separated by semicolons (;).<br>  - You are advised not to set duplicate CNAs.|

### Format of Fault Event Reporting and Reply Messages

#### Alarm ID Corresponding to a Fault Event

| Fault Event| Fault Event ID|
| --- | --- |
| Out-of-band power-off event reporting|1003 (ALARM_REBOOT_EVENT)|
| Out-of-band power-off event reply|1004 (ALARM_REBOOT_ACK_EVENT)|
| OOM event reporting|1005 (ALARM_OOM_EVENT)|
| OOM event reply|1006 (ALARM_OOM_ACK_EVENT)|
| `panic` event reporting|1007 (ALARM_PANIC_EVENT)|
| `panic` event reply|1008 (ALARM_PANIC_ACK_EVENT)|
| In-band `reboot` event reporting|1009 (ALARM_KERNEL_REBOOT_EVENT)|
| In-band `reboot` event reply|1010 (ALARM_KERNEL_REBOOT_ACK_EVENT)|
| UnifiedBus memory fault event|1013 (ALARM_UBUS_MEM_EVENT)|

#### Format of Fault Event Reporting Messages

| Fault Event| Fault Event Message Format| Description|
| --- | --- | ---|
|Out-of-band power-off event| msgid|`msgid`: `uint64_t` type, representing the message ID.|
|OOM event| msgid_{nr_nid:nr_nid,nid:[nid[0],nid[1],...,nid[7]],sync:sync,timeout:timeout,reason:reason} |`msgid`: `uint64_t` type, representing the message ID.<br>  `nr_nid`: `int` type, representing the length of the `nid` array.<br>  `nid[n]`: `int` type, representing the specific value of each entry in the `nid` array.<br>  `sync`: `int` type, representing whether a reply is required (1: reply required; 0: no reply required).<br>  `reason`: `int` type, representing the reason code for the OOM event. For details, refer to the `reclaim_reason` `enum` definition in `linux/mm.h`.|
| `panic` event| msgid_{cna:cna,eid:eid}|`msgid`: `uint64_t` type, representing the message ID.<br>  `cna`: CNA information of the faulty node.<br>  `eid`: EID information of the faulty node.|
| In-band `reboot` event| msgid_{cna:cna,eid:eid}  | `msgid`: `uint64_t` type, representing the message ID.<br>  `cna`: CNA information of the faulty node.<br>  `eid`: EID information of the faulty node.|
| UnifiedBus memory fault event| JSON format| For details, see "Message fields corresponding to UnifiedBus memory fault events". Example: `{"msgid":123,"sentry_ubus_mem_err_type":0,"ras_ubus_mem_err_type":1,"pa":"0x12345678","memid":1}`.|

Message fields corresponding to UnifiedBus memory fault events:

| Parameter| Type| Description|
| --- | --- | ---|
|msgid|Integer|Message ID corresponding to the fault reported by the kernel driver.|
|sentry_ubus_mem_err_type|Integer|Memory fault type reported by Sentry.|
|ras_ubus_mem_err_type|Integer|Detailed fault cause reported by UBUS.|
|pa|String|Physical address that fails to be accessed. The value starting with `0x` indicates a 64-bit hexadecimal integer.|
|memid|Integer|`memid` in the OBMM corresponding to the physical address.|

#### Format of Fault Event Reply Messages

| Fault Event     | Fault Event Reply Message Format| Description|
| ----------------- | -------------------- | ---- |
| Out-of-band power-off event     | msgid_res |`msgid`: `uint64_t` type, representing the message ID.<br>  `res`: `unsigned long` type, representing the message processing result.     |
| OOM event                 |msgid_res |`msgid`: `uint64_t` type, representing the message ID.<br>  `res`: `unsigned long` type, representing the message processing result.     |
| `panic` event        |msgid_{cna:cna,eid:eid}_res                      |`msgid`: `uint64_t` type, representing the message ID.<br>  `cna`: CNA information of the corresponding node.<br>  `eid`: EID information of the corresponding node.<br>  `res`: `unsigned long` type, representing the message processing result.    |
| Kernel `reboot` event|msgid_{cna:cna,eid:eid}_res                      |`msgid`: `uint64_t` type, representing the message ID.<br>  `cna`: CNA information of the corresponding node.<br>  `eid`: EID information of the corresponding node.<br>  `res`: `unsigned long` type, representing the message processing result.    |

### xalarmd Interface

The message receiving and processing program depends on the `xalarmd` service. For details about the required interfaces, see [Secondary Development Guide](./developer_guide.md).
