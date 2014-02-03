make "obj-m := hid-razer.o" -C /lib/modules/$(uname -r)/build M=$PWD hid-razer.ko
