#!/bin/bash

echo 3 1 > /sys/devices/pci0000:80/0000:80:08.0/0000:81:00.0/nvme/nvme0/finject

sleep 10

echo 3 0 > /sys/devices/pci0000:80/0000:80:08.0/0000:81:00.0/nvme/nvme0/finject

killall -u omm

umount /dev/nvme0n1

sleep 2

mount /dev/nvme0n1 /data1

echo 3 0 > /sys/devices/pci0000:80/0000:80:08.0/0000:81:00.0/nvme/nvme0/finject

