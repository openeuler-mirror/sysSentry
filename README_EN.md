# sysSentry

sysSentry is a background inspection framework designed to proactively detect hardware and software faults. By alerting operators before issues escalate into critical production incidents, sysSentry significantly minimizes downtime and enhances overall system reliability.

## Installing sysSentry as a Common User

```bash
yum install -y sysSentry pyxalarm
```

## Installing sysSentry as a Developer

1. Install the build dependencies.

    ```bash
    yum install -y cmake gcc-c++ make python3 python3-setuptools json-c json-c-devel elfutils-devel clang libbpf-devel llvm kernel-source kernel-devel libbpf
    ```

2. Download the source code.

    ```bash
    git clone https://gitee.com/openeuler/sysSentry.git
    ```

3. Compile sysSentry.

    ```bash
    cd sysSentry
    make && make install
    ```

4. (Optional) Compile the plugin.

    ```bash
    cd sysSentry
    make [Plugin] && make install-[Plugin]
    ```

5. Start the service.

    ```bash
    make startup
    ```

6. Delete the software.

    ```bash
    make clean
    ```

## Quick Start

1. Start the inspection framework.

    ```bash
    systemctl start xalarmd
    systemctl start sysSentry
    systemctl start sentryCollector

    ### After the command is executed successfully, you can run the status command to check the status. The status is running.
    systemctl status xalarmd
    systemctl status sysSentry
    systemctl status sentryCollector
    ```

2. Use the `sentryctl` command to manage the framework.

    ```bash
    #Start a specified inspection task.
    sentryctl start <module_name>
    #Terminate a specified inspection task.
    sentryctl stop <module_name>
    #List all loaded inspection tasks and their current status.
    sentryctl list
    #Query the status of a specified inspection task.
    sentryctl status <module_name>
    #Reload the configuration of a specified task.
    sentryctl reload <module_name>
    #View the running result.
    sentryctl get_result <module_name>
    #View alarm information.
    sentryctl get_alarm <module_name>
    ```

For details, see [openEuler documentation](docs.openeuler.org).
