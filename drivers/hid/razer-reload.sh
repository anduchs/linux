echo -n "Please unplug the razer"
while lsusb | grep '1532:0037'>/dev/null 2>&1; do
	sleep 0.1
	echo -n "."
done
rmmod hid_razer; insmod ./hid-razer.ko
echo "Done..."
