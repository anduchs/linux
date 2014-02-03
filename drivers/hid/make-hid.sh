make "hid-y := hid-core.o hid-input.o hidraw.o" -C /lib/modules/$(uname -r)/build M=$PWD hid.ko
