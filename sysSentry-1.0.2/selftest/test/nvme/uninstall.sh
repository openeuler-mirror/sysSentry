

echo "Uninstalling... Please wait for a moment."

if [ -e "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" ]; then
    depmod -aeF "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" "4.19.90-23.48.v2101.ky10.aarch64" > /dev/null || :
else
    depmod "4.19.90-23.48.v2101.ky10.aarch64" > /dev/null || :
fi

modules=( $(cat /var/run/rpm-kmod-hiodriver-modules 2> /dev/null) )
rm -f /var/run/rpm-kmod-hiodriver-modules 2> /dev/null
if [ -x "/sbin/weak-modules" ]; then
    errinfo=$(printf '%%s\n' "${modules[@]}" | /sbin/weak-modules --remove-modules 2>&1 >/dev/null | grep "No space left on device")
    if [ -n "$errinfo" ]; then
        echo "$errinfo"
        echo "Uninstall kmod-hiodriver package failed."
        exit 0
    fi
fi

if [ $1 -ne 0 ]; then
    echo "Uninstall kmod-hiodriver package successfully."
    exit 0
fi

if [ "" != "kdump" ]; then
mods=( $(lsmod | grep -wo "nvme") )
if [ "${mods}" == "nvme" ]; then
    rmmod "nvme" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the installed nvme kernel module."
        echo "Please uninstall the installed nvme kernel module manually or reboot the system."
    else
        echo "Uninstall the installed nvme kernel module successfully."
    fi
fi

standard_nvme=( $(modinfo -n nvme 2>/dev/null) )
if [ -n "${standard_nvme}" ]; then
    mods=( $(lsmod | grep -wo "nvme") )
    if [ -z "${mods}" ]; then
        modprobe  "nvme"  >/dev/null 2>&1
        if [ "$?" -ne 0 ]; then
            echo "Warning: fail to install the in-kernel nvme kernel module."
            echo "Please install the in-kernel nvme kernel module manually or reboot the system."
        else
            echo "Install the in-kernel nvme kernel module successfully."
                    
            #servie name
            serv_name=$(systemctl -a -t service 2>/dev/nul | grep irq | grep balance | sed -e 's/[^ ]* //' | awk '{print $1}' | awk -F'.' '{print $1}')
            if [[ -z ${serv_name} ]]; then
                serv_name=$(chkconfig --list 2>/dev/nul | grep irq | grep balance | awk '{print $1}')
                if [[ -z ${serv_name} ]]; then
                    serv_name=$(ls /etc/init.d/*irq*balance* 2>/dev/nul | awk -F'/' '{print $4}')
                fi
            fi

            if [[ -z ${serv_name} ]]; then
                echo Please start irqbalance service mannually.
            else
                #echo ${serv_name}
                serv_status=$(service ${serv_name} status 2>/dev/nul | grep running)
                #echo ${serv_status}
                if [[ -z ${serv_status} ]]; then
                    echo Please start ${serv_name} service mannually..
                else
                    service ${serv_name} restart 2>/dev/nul
                    if [[ $? -ne 0 ]]; then
                        echo Please start ${serv_name} service mannually...
                    else
                        if [[ "4.19.90-23.48.v2101.ky10.aarch64" < "4.19" ]]; then
                            irqbalance -p 0 --hintpolicy=exact &>/dev/nul
                        fi
                    fi
                fi
            fi
        fi
    fi
fi
fi

echo "Uninstall kmod-hiodriver package successfully."
echo "Please reboot the system"

