/*
 *  HID support for Razer Mice
 *  Copyright 2013, Andreas Fuchs
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <asm/byteorder.h>

#include <linux/hid.h>
#include <linux/usb.h>

#include "hid-ids.h"
#include "hid-razer-private.h"

static int razer_hid_probe(struct hid_device *hdev,
				const struct hid_device_id *id)
{
	int ret;
	struct razer_data *data;

	/* Default handling of input-/event-drivers */
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "Cannot parse device (%d)\n", ret);
		goto error;
	}
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "Cannot start device (%d)\n", ret);
		goto error;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		hid_err(hdev, "Cannot alloc device-driver-memory (%d)\n", ret);
		ret = -ENOMEM;
		goto error_nospecial;
	}
	hid_set_drvdata(hdev, data);

	switch (id->product) {
	case USB_DEVICE_ID_RAZER_DA2013:
		ret = razer_da2013_init(hdev, data);
		if (ret) {
			hid_err(hdev, "Error initializing special functions");
			goto error_free;
		}
		break;
	default:
		hid_warn(hdev, "No special function support implemented for this device\n");
		break;
	}

	return ret;
error_free:
	kfree(data);
	hid_set_drvdata(hdev, NULL);
error_nospecial:
	hid_err(hdev, "Error initializing special function support.");
	ret = 0;
error:
	return ret;
}

static void razer_hid_remove(struct hid_device *hdev)
{
	struct razer_data *data = hid_get_drvdata(hdev);
	if (data) {
		if (data->close)
			data->close(hdev, data);
		kfree(data);
	}
	hid_hw_stop(hdev);
}

static const struct hid_device_id razer_hid_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_RAZER, USB_DEVICE_ID_RAZER_DA2013) },
	{ }
};
MODULE_DEVICE_TABLE(hid, razer_hid_devices);

static struct hid_driver razer_hid_driver = {
	.name = "hid-razer",
	.id_table = razer_hid_devices,
	.probe = razer_hid_probe,
	.remove = razer_hid_remove,
};
module_hid_driver(razer_hid_driver);

MODULE_AUTHOR("Andreas Fuchs");
MODULE_DESCRIPTION("HID driver for Razer-Mice");
MODULE_LICENSE("GPL");
