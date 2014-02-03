(rmmod $(lsmod |grep hid | cut -f 1 -d " ") && insmod ./hid.ko && insmod ./hid-razer.ko) || ((rmmod $(lsmod |grep hid | cut -f 1 -d " ") || true) && modprobe hid)
