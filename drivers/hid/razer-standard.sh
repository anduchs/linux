rmmod $(lsmod |grep hid | cut -f 1 -d " ") &&  modprobe hid
