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

struct razer_data;

/*
 * Support for DeathAdder 2013 Edition from hid-razer-da2013.c
 */

struct razer_da2013_data {
	struct led_classdev *(leds[2]);
};

int razer_da2013_init(struct hid_device *hdev, struct razer_data *data);


/*
 * Common data structure
 */

struct razer_data {
	void (*close) (struct hid_device *hdev, struct razer_data *data);
	union {
		struct razer_da2013_data da2013;
	} specific;
};



