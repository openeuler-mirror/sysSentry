#!/bin/bash

if [ "$(systemctl is-system-running)" = "stopping" ]; then
    echo "[sentry] Detected system shutdown/reboot, keeping the driver loaded."
else
    echo "[sentry] Manual stop, unloading driver"
    /sbin/rmmod sentry_remote_reporter
    if lsmod | grep -q "^sentry_uvb_comm"; then
	    /sbin/rmmod sentry_uvb_comm
    fi
    if lsmod | grep -q "^sentry_urma_comm"; then
	    /sbin/rmmod sentry_urma_comm
    fi
    /sbin/rmmod sentry_reporter
    /sbin/rmmod sentry_msg_helper
    echo "[sentry] unload sentry kmod success"
fi
