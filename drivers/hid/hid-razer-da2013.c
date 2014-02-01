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

struct razer_da2013_cmd {
	u8 header[6];
	u8 cmd[2];
	union {
		u8 buffer[80];
		struct {
			u8 allways_1;
			u8 choice;
			u8 value;
		} led;
		struct {
			u8 value;
			u8 value_repeat;
		} resolution;
	} payload;
	u8 checksum;
	u8 footer;
} __attribute__ ((__packed__));

#define RAZER_DA2013_HEADER_MAGIC {0x00, 0x78, 0x00, 0x00, 0x00, 0x03}
#define RAZER_DA2013_CMD_SET_LED {0x03, 0x00}
#define RAZER_DA2013_CMD_SET_RESOLUTION {0x04, 0x01}

/* TODO: what is a good prefix for led-names ?
#define RAZER_DA2013_LED_PREFIX devname(dev)*/
#define RAZER_DA2013_LED_PREFIX "led"
#define RAZER_DA2013_LED1 "::wheel"
#define RAZER_DA2013_LED2 "::logo"

/* TODO: Implement getter for led status
static enum led_brightness razer_da2013_led_get(struct led_classdev *led_dev)
{
	struct device *dev = led_dev->dev->parent;
	...
	return 0;
} */

static void razer_da2013_led_set(struct led_classdev *led_dev,
			   enum led_brightness value)
{
	ssize_t len;

	struct device *dev = led_dev->dev->parent;
	struct usb_device *usbdev = interface_to_usbdev(
			to_usb_interface(dev->parent));
	struct razer_data *data = hid_get_drvdata(
			container_of(dev, struct hid_device, dev));

	struct razer_da2013_cmd cmd = {
				.header = RAZER_DA2013_HEADER_MAGIC,
				.cmd = RAZER_DA2013_CMD_SET_LED,
				.payload.led.allways_1 = 0x01,
				.payload.led.value = value,
			};

	int led_index = sizeof(data->specific.da2013.leds) /
			sizeof(data->specific.da2013.leds[0]);
	for (led_index--; led_index >= 0; led_index--)
		if (data->specific.da2013.leds[led_index] == led_dev)
			break;

	if (led_index == 0) {
		cmd.payload.led.choice = 0x01;
		cmd.checksum = value ? 0x01 : 0x00;
	} else if (led_index == 1) {
		cmd.payload.led.choice = 0x04;
		cmd.checksum = value ? 0x04 : 0x05;
	} else {
		dev_err(dev, "Unknown LED index\n");
		return;
	}

	len = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			USB_REQ_SET_CONFIGURATION,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT,
			0x300,
			0, &cmd, sizeof(cmd), USB_CTRL_SET_TIMEOUT);

	if (len != sizeof(cmd)) {
		dev_err(dev, "Sending Control (%zd)\n", len);
		return;
	}
}

static ssize_t resolution_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	/* TODO: Lookup status via usb... */
	dev_err(dev, "Get-Resolution not implemented\n");
	return sprintf(buf, "0\n");
}

static ssize_t resolution_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct usb_device *usbdev = interface_to_usbdev(
			to_usb_interface(dev->parent));

	ssize_t len, ret = count;

	struct razer_da2013_cmd cmd = {
				.header = RAZER_DA2013_HEADER_MAGIC,
				.cmd = RAZER_DA2013_CMD_SET_RESOLUTION,
				.checksum = 0x06,
			};

	if (sscanf(buf, "%hhd", &cmd.payload.resolution.value) != 1) {
		dev_err(dev, "No parsable value for resolution");
		ret = -EIO;
		goto error;
	}

	cmd.payload.resolution.value_repeat = cmd.payload.resolution.value;

	len = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			USB_REQ_SET_CONFIGURATION,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT,
			0x300,
			0, &cmd, sizeof(cmd), USB_CTRL_SET_TIMEOUT);

	if (len != sizeof(cmd)) {
		dev_err(dev, "Sending Control (%zd)\n", len);
		ret = usb_translate_errors(len);
	}

error:
	return ret;

}

static DEVICE_ATTR(resolution,  S_IRUGO | S_IWUSR | S_IWGRP,
		resolution_show, resolution_store);

static void razer_da2013_close(struct hid_device *hdev, struct razer_data *data)
{
	led_classdev_unregister(data->specific.da2013.leds[1]);
	led_classdev_unregister(data->specific.da2013.leds[0]);
	kfree(data->specific.da2013.leds[1]);
	kfree(data->specific.da2013.leds[0]);
	device_remove_file(&hdev->dev, &dev_attr_resolution);
}

int razer_da2013_init(struct hid_device *hdev, struct razer_data *data)
{
	int ret;
	int size;
	struct device *dev = &hdev->dev;
	const char *prefix = RAZER_DA2013_LED_PREFIX;

	/* Add special attributes only for the mouse, not the two keyboards. */
	if (to_usb_interface(hdev->dev.parent)
			->cur_altsetting->desc.bInterfaceProtocol
				!= USB_INTERFACE_PROTOCOL_MOUSE)
		return 0;

	data->close = razer_da2013_close;

	/*Add LED at Scroll-Wheel and Logo */
	size = strlen(prefix) + strlen(RAZER_DA2013_LED1) + 1;
	data->specific.da2013.leds[0] = kzalloc(sizeof(struct led_classdev) +
			size, GFP_KERNEL);
	if (data->specific.da2013.leds[0] == NULL) {
		hid_err(hdev, "cannot allocate memory\n");
		ret = -ENOMEM;
		goto error;
	}
	data->specific.da2013.leds[0]->name =
			(const char *) &data->specific.da2013.leds[0][1];
	if (snprintf((char *)data->specific.da2013.leds[0]->name, size,
			"%s" RAZER_DA2013_LED1, prefix) < 0)  {
		hid_err(hdev, "cannot construct led names\n");
		ret = -1;
		goto error_free1;
	}
	data->specific.da2013.leds[0]->brightness = 1;
	data->specific.da2013.leds[0]->max_brightness = 1;
	/* TODO: Implement getter for led status
	data->specific.da2013.leds[0]->brightness_get = razer_da2013_led_get; */
	data->specific.da2013.leds[0]->brightness_set = razer_da2013_led_set;

	size = strlen(prefix) + strlen(RAZER_DA2013_LED2) + 1;
	data->specific.da2013.leds[1] = kzalloc(sizeof(struct led_classdev) +
			size, GFP_KERNEL);
	if (data->specific.da2013.leds[1] == NULL) {
		hid_err(hdev, "cannot allocate memory\n");
		ret = -ENOMEM;
		goto error_free1;
	}
	data->specific.da2013.leds[1]->name =
			(const char *) &data->specific.da2013.leds[1][1];
	if (snprintf((char *)data->specific.da2013.leds[1]->name, size,
			"%s" RAZER_DA2013_LED2, prefix) < 0)  {
		hid_err(hdev, "cannot construct led names\n");
		ret = -1;
		goto error_free2;
	}
	data->specific.da2013.leds[1]->brightness = 1;
	data->specific.da2013.leds[1]->max_brightness = 1;
	/* TODO: Implement getter for led status
	data->specific.da2013.leds[1]->brightness_get = razer_da2013_led_get; */
	data->specific.da2013.leds[1]->brightness_set = razer_da2013_led_set;

	ret = led_classdev_register(dev, data->specific.da2013.leds[0]);
	if (ret) {
		hid_err(hdev, "cannot register led device\n");
		goto error_free2;
	}

	ret = led_classdev_register(dev, data->specific.da2013.leds[1]);
	if (ret) {
		hid_err(hdev, "cannot register led device\n");
		goto error_unregister1;
	}

	/* Add the device attribute for DPI settings */
	ret = device_create_file(dev, &dev_attr_resolution);
	if (ret) {
		hid_err(hdev, "cannot create sysfs attribute\n");
		goto error_unregister2;
	}

	return ret;
error_unregister2:
	led_classdev_unregister(data->specific.da2013.leds[1]);
error_unregister1:
	led_classdev_unregister(data->specific.da2013.leds[0]);
error_free2:
	kfree(data->specific.da2013.leds[1]);
error_free1:
	kfree(data->specific.da2013.leds[0]);
error:
	return ret;
}
