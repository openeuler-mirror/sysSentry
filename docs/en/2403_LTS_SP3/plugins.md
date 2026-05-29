# Plugin Introduction

The `sysSentry` inspection framework manages various inspection capabilities, all of which are implemented as plugins. Different plugins support different inspection functions. The plugins currently provided by `sysSentry` are as follows:

- Slow I/O plugin: Performs slow drive fault detection. When the inspection plugin detects a slow drive in the system, it reports the results to the `xalarmd` service. Users can view alarm results by subscribing to alarms or using the `get_alarm` command.

- UBPRM: Implements monitoring and reporting for events such as `panic`, `reboot`, BMC power-off, OOM, and UnifiedBus memory faults.
