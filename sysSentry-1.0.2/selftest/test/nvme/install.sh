echo "Installing... Please wait for a moment."
if lsmod | grep -wo "crct10dif_neon";then
        echo "add_drivers+=\"crct10dif-neon\"" > /etc/dracut.conf.d/hiodriver.conf
else
        echo "add_drivers+=\"crct10dif-ce\"" > /etc/dracut.conf.d/hiodriver.conf
fi

if [ -e "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" ]; then
    depmod -aeF "/boot/System.map-4.19.90-23.48.v2101.ky10.aarch64" "4.19.90-23.48.v2101.ky10.aarch64"  || :
fi

modules=( $(find /lib/modules/4.19.90-23.48.v2101.ky10.aarch64/extra/hiodriver | grep '\.ko$') )
if [ -x "/sbin/weak-modules" ]; then
    errinfo=$(printf '%%s\n' "${modules[@]}" | /sbin/weak-modules --add-modules 2>&1  | grep "No space left on device")
    if [ -n "$errinfo" ]; then
        echo "$errinfo"
        echo "Install kmod-hiodriver package failed."
        exit 1
    fi
fi


# if [ -d "/lib/modules/4.19.90-23.48.v2101.ky10.aarch64/kernel" ]; then
#   modinfo=( $(modinfo -k 4.19.90-23.48.v2101.ky10.aarch64 nvme 2>/dev/null | grep -w "Huawei") )
# else
#   modinfo=( $(modinfo nvme 2>/dev/null | grep -w "Huawei") )
# fi
# if [ -z "${modinfo}" ]; then
#   echo "Error: The installing package does not match the current OS."
#   echo "Please execute command: rpm -e kmod-hiodriver to uninstall the installing rpm."
#   exit 0
# fi

if [ "" != "kdump" ]; then
mods=( $(lsmod | grep -wo "nvme") )
if [ "${mods}" == "nvme" ]; then
    rmmod "nvme" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the old nvme kernel module."
        echo "Please uninstall the old nvme kernel module manually or reboot the system."
        exit 1
    else
        echo "Uninstall the old nvme kernel module successfully."
    fi
fi
mods=( $(lsmod | grep -wo "nvme_core") )
if [ "${mods}" == "nvme_core" ]; then
    rmmod "nvme_core" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to uninstall the old nvme-core kernel module."
        exit 1
    else
        echo "Uninstall the nvme_core kernel module successfully."
    fi
fi

mods=( $(lsmod | grep -o "^nvme[ ]") )
if [ -z "${mods}" ]; then
    modprobe  "nvme" 
    if [ "$?" -ne 0 ]; then
        echo "Warning: fail to install the new nvme kernel module."
        echo "Please install the new nvme kernel module manually or reboot the system."
        exit 1
    else
        echo "Install the new nvme kernel module successfully."
        
        #servie name
        serv_name=$(systemctl -a -t service  | grep irq | grep balance | sed -e 's/[^ ]* //' | awk '{print $1}' | awk -F'.' '{print $1}')
        if [[ -z ${serv_name} ]]; then
            serv_name=$(chkconfig --list  | grep irq | grep balance | awk '{print $1}')
            if [[ -z ${serv_name} ]]; then
                serv_name=$(ls /etc/init.d/*irq*balance*  | awk -F'/' '{print $4}')
            fi
        fi

        if [[ -z ${serv_name} ]]; then
            echo Please start irqbalance service mannually.
        else
            #echo ${serv_name}
            serv_status=$(service ${serv_name} status  | grep running)
            #echo ${serv_status}
            if [[ -z ${serv_status} ]]; then
                echo Please start ${serv_name} service mannually..
            else
                sleep 10
                service ${serv_name} restart 
                if [[ $? -ne 0 ]]; then
                    echo Please start ${serv_name} service mannually...
                else
                    if [[ "4.19.90-23.48.v2101.ky10.aarch64" < "4.19" ]]; then
                        irqbalance -p 0 --hintpolicy=exact 
                    fi
                fi
            fi
        fi
    fi
fi


kdump=($(ls /boot/|grep kdump|grep init))
if [ -z "${kdump}" ];then
        echo ""
else
        echo "Modify kdump image.Please wait for a moment...50%%"
        echo "$kdump"
        mkdumprd -f /boot/$kdump  
fi
fi

echo "Install kmod-hiodriver package successfully."



