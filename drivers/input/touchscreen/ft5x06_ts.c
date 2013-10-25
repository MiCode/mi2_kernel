/*
 * Copyright (C) 2011 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/ft5x06_ts.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/input/mt.h>
#include "ft5x06_ts.h"

//register address
#define FT5X0X_REG_DEVIDE_MODE	0x00
#define FT5X0X_REG_ROW_ADDR		0x01
#define FT5X0X_REG_TD_STATUS		0x02
#define FT5X0X_REG_START_SCAN		0x02
#define FT5X0X_REG_TOUCH_START	0x03
#define FT5X0X_REG_VOLTAGE		0x05
#define FT5X0X_ID_G_PMODE			0xA5
#define FT5x0x_REG_FW_VER			0xA6
#define FT5x0x_ID_G_FT5201ID		0xA8
#define FT5X0X_NOISE_FILTER		0xB5
#define FT5x0x_REG_POINT_RATE		0x88
#define FT5X0X_REG_THGROUP		0x80
#define FT5X0X_REG_RESET			0xFC

#define FT5X0X_DEVICE_MODE_NORMAL	0x00
#define FT5X0X_DEVICE_MODE_TEST	0x40
#define FT5X0X_DEVICE_START_CALIB	0x04
#define FT5X0X_DEVICE_SAVE_RESULT	0x05

#define FT5X0X_POWER_ACTIVE             0x00
#define FT5X0X_POWER_MONITOR            0x01
#define FT5X0X_POWER_HIBERNATE          0x03


/* ft5x0x register list */
#define FT5X0X_TOUCH_LENGTH		6

#define FT5X0X_TOUCH_XH			0x00 /* offset from each touch */
#define FT5X0X_TOUCH_XL			0x01
#define FT5X0X_TOUCH_YH			0x02
#define FT5X0X_TOUCH_YL			0x03
#define FT5X0X_TOUCH_PRESSURE		0x04
#define FT5X0X_TOUCH_SIZE		0x05

/* ft5x0x bit field definition */
#define FT5X0X_MODE_NORMAL		0x00
#define FT5X0X_MODE_SYSINFO		0x10
#define FT5X0X_MODE_TEST		0x40
#define FT5X0X_MODE_MASK		0x70

#define FT5X0X_EVENT_DOWN		0x00
#define FT5X0X_EVENT_UP			0x40
#define FT5X0X_EVENT_CONTACT		0x80
#define FT5X0X_EVENT_MASK		0xc0


/* ft5x0x firmware upgrade definition */
#define FT5X0X_FIRMWARE_TAIL		-8 /* base on the end of firmware */
#define FT5X0X_FIRMWARE_VERION		-2
#define FT5X0X_PACKET_HEADER		6
#define FT5X0X_PACKET_LENGTH		128

/* ft5x0x absolute value */
#define FT5X0X_MAX_FINGER		0x0A
#define FT5X0X_MAX_SIZE			0xff
#define FT5X0X_MAX_PRESSURE		0xff

#define NOISE_FILTER_DELAY	HZ

struct ft5x06_packet {
	u8  magic1;
	u8  magic2;
	u16 offset;
	u16 length;
	u8  payload[FT5X0X_PACKET_LENGTH];
};

struct ft5x06_finger {
	int x, y;
	int size;
	int pressure;
	bool detect;
};

struct ft5x06_tracker {
	int x, y;
	bool detect;
	bool moving;
	unsigned long jiffies;
};

struct ft5x06_data {
	struct mutex mutex;
	struct device *dev;
	struct input_dev *input;
	struct kobject *vkeys_dir;
	struct kobj_attribute vkeys_attr;
	struct notifier_block power_supply_notifier;
	const struct ft5x06_bus_ops *bops;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct ft5x06_tracker tracker[FT5X0X_MAX_FINGER];
	int  irq;
	bool dbgdump;
	unsigned int test_result;
	bool in_suspend;
	struct delayed_work noise_filter_delayed_work;
};

static int ft5x06_recv_byte(struct ft5x06_data *ft5x06, u8 len, ...)
{
	int error;
	va_list varg;
	u8 i, buf[len];

	error = ft5x06->bops->recv(ft5x06->dev, buf, len);
	if (error)
		return error;

	va_start(varg, len);
	for (i = 0; i < len; i++)
		*va_arg(varg, u8 *) = buf[i];
	va_end(varg);

	return 0;
}

static int ft5x06_send_block(struct ft5x06_data *ft5x06,
				const void *buf, int len)
{
	return ft5x06->bops->send(ft5x06->dev, buf, len);
}

static int ft5x06_send_byte(struct ft5x06_data *ft5x06, u8 len, ...)
{
	va_list varg;
	u8 i, buf[len];

	va_start(varg, len);
	for (i = 0; i < len; i++)
		buf[i] = va_arg(varg, int); /* u8 promote to int */
	va_end(varg);

	return ft5x06_send_block(ft5x06, buf, len);
}

static int ft5x06_read_block(struct ft5x06_data *ft5x06,
				u8 addr, void *buf, u8 len)
{
	return ft5x06->bops->read(ft5x06->dev, addr, buf, len);
}

static int ft5x06_read_byte(struct ft5x06_data *ft5x06, u8 addr, u8 *data)
{
	return ft5x06_read_block(ft5x06, addr, data, sizeof(*data));
}

static int ft5x06_write_byte(struct ft5x06_data *ft5x06, u8 addr, u8 data)
{
	return ft5x06->bops->write(ft5x06->dev, addr, &data, sizeof(data));
}

static int reset_delay[] = {
	30, 33, 36, 39, 42, 45, 27, 24, 21, 18, 15
};

static void ft5x06_charger_state_changed(struct ft5x06_data *ft5x06)
{
	u8 val;
	u8 is_usb_exist;
	ft5x06_read_byte(ft5x06, FT5X0X_NOISE_FILTER, &val);
	is_usb_exist = power_supply_is_system_supplied();
	if (val != is_usb_exist) {
		dev_info(ft5x06->dev, "Power state changed, set noise filter to 0x%x\n", is_usb_exist);
		ft5x06_write_byte(ft5x06, FT5X0X_NOISE_FILTER, is_usb_exist);
	}
}

static void ft5x06_noise_filter_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ft5x06_data *ft5x06 = container_of(delayed_work, struct ft5x06_data, noise_filter_delayed_work);

	dev_info(ft5x06->dev, "ft5x06_noise_filter_delayed_work called\n");
	ft5x06_charger_state_changed(ft5x06);
}

static int ft5x06_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct ft5x06_data *ft5x06 = container_of(nb, struct ft5x06_data, power_supply_notifier);

	if (ft5x06->dbgdump)
		dev_info(ft5x06->dev, "Power_supply_event\n");
	if (!ft5x06->in_suspend)
		ft5x06_charger_state_changed(ft5x06);
	else if (ft5x06->dbgdump)
		dev_info(ft5x06->dev, "Don't response to power supply event in suspend mode!\n");

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
static int ft5x06_auto_calib(struct ft5x06_data *ft5x06)
{
	int error;
	u8 val1;
	int i;
	msleep(200);
	error = ft5x06_write_byte(ft5x06, /* enter factory mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x06_write_byte(ft5x06, /* start calibration */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_START_CALIB);
	if (error)
		return error;
	msleep(300);

	for (i = 0; i < 100; i++) {
		error = ft5x06_read_byte(ft5x06, FT5X0X_REG_DEVIDE_MODE, &val1);
		if (error)
			return error;
		if ((val1&0x70) == 0) /* return to normal mode? */
			break;
		msleep(200); /* not yet, wait and try again later */
	}
	dev_info(ft5x06->dev, "[FTS] calibration OK.\n");

	msleep(300); /* enter factory mode again */
	error = ft5x06_write_byte(ft5x06, FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x06_write_byte(ft5x06, /* save calibration result */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_SAVE_RESULT);
	if (error)
		return error;
	msleep(300);

	error = ft5x06_write_byte(ft5x06, /* return to normal mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x06->dev, "[FTS] Save calib result OK.\n");

	return 0;
}
#endif

static u8 ft5x06_get_factory_id(struct ft5x06_data *ft5x06)
{
	int error = 0;
	int i;
	u8 reg_val[2];
	u8 buf[6];
	/* step 1: Reset CTPM */
	error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0xaa);
	if (error)
		return error;
	msleep(50);
	error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0x55);
	if (error)
		return error;
	dev_info(ft5x06->dev, "Step 1: Reset CTPM. \n");
	msleep(30);

	/* step 2: Enter upgrade mode */
	i = 0;
	do {
		error = ft5x06_send_byte(ft5x06, 2, 0x55, 0xaa);
		msleep(5);
	} while (error && i < 5);

	/* step 3: Check READ-ID */
	error = ft5x06_send_byte(ft5x06, 4, 0x90, 0x00, 0x00, 0x00);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 2, &reg_val[0], &reg_val[1]);
	if (error)
		return error;
	if (reg_val[0] == 0x79 && reg_val[1] == 0x03)
		dev_info(ft5x06->dev, "Step 3:  CTPM id0 = 0x%x, id1 = 0x%x. \n",
			reg_val[0], reg_val[1]);
	else
		return -1;
	error = ft5x06_send_byte(ft5x06, 1, 0xCD);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 1, &reg_val[0]);
	if (error)
		return error;
	dev_info(ft5x06->dev, "bootloader version = 0x%x\n", reg_val[0]);

	/* read current project setting */
	error = ft5x06_send_byte(ft5x06, 4, 0x03, 0x00, 0x78, 0x00);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 6, &buf[0], &buf[1],
				&buf[2], &buf[3],
				&buf[4], &buf[5]);
	if (error)
		return error;
	error = ft5x06_send_byte(ft5x06, 1, 0x07);
	if (error)
		return error;
	msleep(200);

	return buf[4];

}

static int ft5x06_load_firmware(struct ft5x06_data *ft5x06,
		const struct ft5x06_firmware_data *firmware, bool *upgraded)
{
	struct ft5x06_packet packet;
	int i, j, length, error = 0;
	u8 val1, val2, id, ecc = 0;
#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
	const int max_calib_time = 3;
	bool calib_ok = false;
#endif

	/* step 0a: check and init argument */
	if (upgraded)
		*upgraded = false;

	if (firmware == NULL)
		return 0;

	/* step 0b: find the right firmware for touch screen */
	error = ft5x06_read_byte(ft5x06, FT5x0x_ID_G_FT5201ID, &id);
	if (error)
		return error;
	dev_info(ft5x06->dev, "firmware vendor is %02x\n", id);
	if (id == FT5x0x_ID_G_FT5201ID) {
		id = ft5x06_get_factory_id(ft5x06);
		dev_err(ft5x06->dev, "firmware corruption, read real factory id = 0x%x!\n", id);
	}

	for (; firmware->size != 0; firmware++) {
		if (id == firmware->vendor)
			break;
	}

	if (firmware->size == 0) {
		dev_err(ft5x06->dev, "unknown touch screen vendor, failed!\n");
		return -ENOENT;
	}

	/* step 1: check firmware id is different */
	error = ft5x06_read_byte(ft5x06, FT5x0x_REG_FW_VER, &id);
	if (error)
		return error;
	dev_info(ft5x06->dev, "firmware version is %02x\n", id);

	if (id == firmware->data[firmware->size+FT5X0X_FIRMWARE_VERION])
		return 0;
	dev_info(ft5x06->dev, "upgrade firmware to %02x\n",
		firmware->data[firmware->size+FT5X0X_FIRMWARE_VERION]);
	dev_info(ft5x06->dev, "[FTS] step1: check fw id\n");

	for (i = 0, error = -1; i < ARRAY_SIZE(reset_delay) && error; i++) {
		/* step 2: reset device */
		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(50);

		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(reset_delay[i]);
		dev_info(ft5x06->dev, "[FTS] step2: Reset device.\n");

		/* step 3: enter upgrade mode */
		for (i = 0; i < 10; i++) {
			error = ft5x06_send_byte(ft5x06, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;
		dev_info(ft5x06->dev, "[FTS] step3: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x06_send_byte(ft5x06, 4, 0x90, 0x00, 0x00, 0x00);
		if (error)
			continue;

		error = ft5x06_recv_byte(ft5x06, 2, &val1, &val2);
		if (error)
			continue;

		if (val1 != 0x79 || val2 != 0x03)
			error = -ENODEV;
		dev_info(ft5x06->dev, "[FTS] step4: Check device id.\n");
	}

	if (error) /* check the final result */
		return error;

	error = ft5x06_send_byte(ft5x06, 1, 0xcd);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 1, &val1);
	if (error)
		return error;
	dev_info(ft5x06->dev, "[FTS] bootloader version is 0x%x\n", val1);

	/* step 5: erase device */
	error = ft5x06_send_byte(ft5x06, 1, 0x61);
	if (error)
		return error;
	msleep(1500);
	error = ft5x06_send_byte(ft5x06, 1, 0x63);
	if (error)
		return error;
	msleep(100);
	dev_info(ft5x06->dev, "[FTS] step5: Erase device.\n");

	/* step 6: flash firmware to device */
	packet.magic1 = 0xbf;
	packet.magic2 = 0x00;
	/* step 6a: send data in 128 bytes chunk each time */
	for (i = 0; i < firmware->size+FT5X0X_FIRMWARE_TAIL; i += length) {
		length = min(FT5X0X_PACKET_LENGTH,
				firmware->size+FT5X0X_FIRMWARE_TAIL-i);

		packet.offset = cpu_to_be16(i);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = firmware->data[i+j];
			ecc ^= firmware->data[i+j];
		}

		error = ft5x06_send_block(ft5x06, &packet,
					FT5X0X_PACKET_HEADER+length);
		if (error)
			return error;

		msleep(FT5X0X_PACKET_LENGTH/6);
	}
	dev_info(ft5x06->dev, "[FTS] step6a: Send data in 128 bytes chunk each time.\n");

	/* step 6b: send one byte each time for last six bytes */
	for (j = 0; i < firmware->size+FT5X0X_FIRMWARE_VERION; i++, j++) {
		packet.offset = cpu_to_be16(0x6ffa+j);
		packet.length = cpu_to_be16(1);

		packet.payload[0] = firmware->data[i];
		ecc ^= firmware->data[i];

		error = ft5x06_send_block(ft5x06, &packet,
					FT5X0X_PACKET_HEADER+1);
		if (error)
			return error;

		msleep(20);
	}
	dev_info(ft5x06->dev, "[FTS] step6b: Send one byte each time for last six bytes.\n");

	/* step 7: verify checksum */
	error = ft5x06_send_byte(ft5x06, 1, 0xcc);
	if (error)
		return error;

	error = ft5x06_recv_byte(ft5x06, 1, &val1);
	if (error)
		return error;

	if (val1 != ecc)
		return -ERANGE;
	dev_info(ft5x06->dev, "[FTS] step7:Verify checksum.\n");

	/* step 8: reset to new firmware */
	error = ft5x06_send_byte(ft5x06, 1, 0x07);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x06->dev, "[FTS] step8: Reset to new firmware.\n");

#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
	/* step 9: calibrate the reference value */
	for (i = 0; i < max_calib_time; i++) {
		error = ft5x06_auto_calib(ft5x06);
		if (!error) {
			calib_ok = true;
			dev_info(ft5x06->dev, "[FTS] step9: Calibrate the ref value successfully.\n");
			break;
		}
	}
	if (!calib_ok) {
		dev_info(ft5x06->dev, "[FTS] step9: Calibrate the ref value failed.\n");
		return error;
	}
#endif

	if (upgraded)
		*upgraded = true;

	return 0;
}

static int ft5x06_collect_finger(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
	u8 number, buf[256];
	int i, error;

	error = ft5x06_read_byte(ft5x06, FT5X0X_REG_TD_STATUS, &number);
	if (error)
		return error;
	number &= 0x0f;

	if (number > FT5X0X_MAX_FINGER)
		number = FT5X0X_MAX_FINGER;

	error = ft5x06_read_block(ft5x06, FT5X0X_REG_TOUCH_START,
				buf, FT5X0X_TOUCH_LENGTH*number);
	if (error)
		return error;

	/* clear the finger buffer */
	memset(finger, 0, sizeof(*finger)*count);

	for (i = 0; i < number; i++) {
		u8 xh = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_XH];
		u8 xl = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_XL];
		u8 yh = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_YH];
		u8 yl = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_YL];

		u8 size     = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_SIZE];
		u8 pressure = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_PRESSURE];

		u8 id = (yh&0xf0)>>4;

		finger[id].x        = ((xh&0x0f)<<8)|xl;
		finger[id].y        = ((yh&0x0f)<<8)|yl;
		finger[id].size     = size;
		finger[id].pressure = pressure;
		finger[id].detect   = (xh&FT5X0X_EVENT_MASK) != FT5X0X_EVENT_UP;

		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev,
				"fig(%02u): %d %04d %04d %03d %03d\n", id,
				finger[i].detect, finger[i].x, finger[i].y,
				finger[i].pressure, finger[i].size);
	}

	return 0;
}

static void ft5x06_apply_filter(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int i;

	for (i = 0; i < count; i++) {
		if (!finger[i].detect) /* finger release */
			ft5x06->tracker[i].detect = false;
		else if (!ft5x06->tracker[i].detect) { /* initial touch */
			ft5x06->tracker[i].x = finger[i].x;
			ft5x06->tracker[i].y = finger[i].y;
			ft5x06->tracker[i].detect  = true;
			ft5x06->tracker[i].moving  = false;
			ft5x06->tracker[i].jiffies = jiffies;
		} else { /* the rest report until finger lift */
			unsigned long landed_jiffies;
			int delta_x, delta_y, threshold;

			landed_jiffies  = ft5x06->tracker[i].jiffies;
			landed_jiffies += pdata->landing_jiffies;

			/* no significant movement yet */
			if (!ft5x06->tracker[i].moving) {
				/* use the big threshold for landing period */
				if (time_before(jiffies, landed_jiffies))
					threshold = pdata->landing_threshold;
				else /* use the middle jitter threshold */
					threshold = pdata->staying_threshold;
			} else { /* use the small threshold during movement */
				threshold = pdata->moving_threshold;
			}

			delta_x = finger[i].x - ft5x06->tracker[i].x;
			delta_y = finger[i].y - ft5x06->tracker[i].y;

			delta_x *= delta_x;
			delta_y *= delta_y;

			/* use the saved value for small change */
			if (delta_x + delta_y <= threshold * threshold)
			{
				finger[i].x = ft5x06->tracker[i].x;
				finger[i].y = ft5x06->tracker[i].y;
			} else {/* save new location */
				ft5x06->tracker[i].x = finger[i].x;
				ft5x06->tracker[i].y = finger[i].y;
				ft5x06->tracker[i].moving = true;
			}
		}
	}
}

static void ft5x06_report_touchevent(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	bool mt_sync_sent = false;
#endif
	int i;

	for (i = 0; i < count; i++) {
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_slot(ft5x06->input, i);
#endif
		if (!finger[i].detect) {
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
			input_mt_report_slot_state(ft5x06->input, MT_TOOL_FINGER, 0);
			input_report_abs(ft5x06->input, ABS_MT_TRACKING_ID, -1);
#endif
			continue;
		}

#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_report_slot_state(ft5x06->input, MT_TOOL_FINGER, 1);
#endif
		input_report_abs(ft5x06->input, ABS_MT_TRACKING_ID, i);
		input_report_abs(ft5x06->input, ABS_MT_POSITION_X ,
			max(1, finger[i].x)); /* for fruit ninja */
		input_report_abs(ft5x06->input, ABS_MT_POSITION_Y ,
			max(1, finger[i].y)); /* for fruit ninja */
		input_report_abs(ft5x06->input, ABS_MT_TOUCH_MAJOR,
			max(1, finger[i].pressure));
		input_report_abs(ft5x06->input, ABS_MT_WIDTH_MAJOR,
			max(1, finger[i].size));
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_sync(ft5x06->input);
		mt_sync_sent = true;
#endif
		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev,
				"tch(%02d): %04d %04d %03d %03d\n",
				i, finger[i].x, finger[i].y,
				finger[i].pressure, finger[i].size);
	}
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	if (!mt_sync_sent) {
		input_mt_sync(ft5x06->input);
		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev, "tch(xx): no touch contact\n");
	}
#endif

	input_sync(ft5x06->input);
}

static irqreturn_t ft5x06_interrupt(int irq, void *dev_id)
{
	struct ft5x06_finger finger[FT5X0X_MAX_FINGER];
	struct ft5x06_data *ft5x06 = dev_id;
	int error;

	mutex_lock(&ft5x06->mutex);
	error = ft5x06_collect_finger(ft5x06, finger, FT5X0X_MAX_FINGER);
	if (error >= 0) {
		ft5x06_apply_filter(ft5x06, finger, FT5X0X_MAX_FINGER);
		ft5x06_report_touchevent(ft5x06, finger, FT5X0X_MAX_FINGER);
	} else
		dev_err(ft5x06->dev, "fail to collect finger(%d)\n", error);
	mutex_unlock(&ft5x06->mutex);

	return IRQ_HANDLED;
}

int ft5x06_suspend(struct ft5x06_data *ft5x06)
{
	int error = 0;

	mutex_lock(&ft5x06->mutex);
	memset(ft5x06->tracker, 0, sizeof(ft5x06->tracker));

	ft5x06->in_suspend = true;
	cancel_delayed_work_sync(&ft5x06->noise_filter_delayed_work);
	error = ft5x06_write_byte(ft5x06,
			FT5X0X_ID_G_PMODE, FT5X0X_POWER_HIBERNATE);

	mutex_unlock(&ft5x06->mutex);

	return error;
}
EXPORT_SYMBOL_GPL(ft5x06_suspend);

int ft5x06_resume(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;

	mutex_lock(&ft5x06->mutex);

	/* reset device */
	gpio_set_value_cansleep(pdata->reset_gpio, 0);
	msleep(1);
	gpio_set_value_cansleep(pdata->reset_gpio, 1);
	msleep(50);

	schedule_delayed_work(&ft5x06->noise_filter_delayed_work,
				NOISE_FILTER_DELAY);
	ft5x06->in_suspend = false;
	mutex_unlock(&ft5x06->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(ft5x06_resume);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_early_suspend(struct early_suspend *h)
{
	struct ft5x06_data *ft5x06 = container_of(h,
					struct ft5x06_data, early_suspend);
	ft5x06_suspend(ft5x06);
}

static void ft5x06_early_resume(struct early_suspend *h)
{
	struct ft5x06_data *ft5x06 = container_of(h,
					struct ft5x06_data, early_suspend);
	ft5x06_resume(ft5x06);
}
#endif

static ssize_t ft5x06_vkeys_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 =
		container_of(attr, struct ft5x06_data, vkeys_attr);
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	const struct ft5x06_keypad_data *keypad = pdata->keypad;
	int i, count = 0;

	for (i = 0; keypad && i < keypad->length; i++) {
		int width  = keypad->button[i].width;
		int height = keypad->button[i].height;
		int midx   = keypad->button[i].left+width/2;
		int midy   = keypad->button[i].top+height/2;

		count += snprintf(buf+count, PAGE_SIZE-count,
				"0x%02x:%d:%d:%d:%d:%d:",
				EV_KEY, keypad->keymap[i],
				midx, midy, width, height);
	}

	count -= 1; /* remove the last colon */
	count += snprintf(buf+count, PAGE_SIZE-count, "\n");
	return count;
}

static ssize_t ft5x06_object_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	static struct {
		u8 addr;
		const char *fmt;
	} reg_list[] = {
		/* threshold setting */
		{0x80, "THGROUP          %3d\n"  },
		{0x81, "THPEAK           %3d\n"  },
		{0x82, "THCAL            %3d\n"  },
		{0x83, "THWATER          %3d\n"  },
		{0x84, "THTEMP           %3d\n"  },
		{0x85, "THDIFF           %3d\n"  },
		{0xae, "THBAREA          %3d\n"  },
		/* mode setting */
		{0x86, "CTRL              %02x\n"},
		{0xa0, "AUTOCLB           %02x\n"},
		{0xa4, "MODE              %02x\n"},
		{0xa5, "PMODE             %02x\n"},
		{0xa7, "STATE             %02x\n"},
		{0xa9, "ERR               %02x\n"},
		/* timer setting */
		{0x87, "TIME2MONITOR     %3d\n"  },
		{0x88, "PERIODACTIVE     %3d\n"  },
		{0x89, "PERIODMONITOR    %3d\n"  },
		/* version info */
		{0xa1, "LIBVERH           %02x\n"},
		{0xa2, "LIBVERL           %02x\n"},
		{0xa3, "CIPHER            %02x\n"},
		{0xa6, "FIRMID            %02x\n"},
		{0xa8, "FT5201ID          %02x\n"},
		{/* end of the list */},
	};

	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int i, error, count = 0;
	u8 val;

	mutex_lock(&ft5x06->mutex);
	for (i = 0; reg_list[i].addr != 0; i++) {
		error = ft5x06_read_byte(ft5x06, reg_list[i].addr, &val);
		if (error)
			break;

		count += snprintf(buf+count, PAGE_SIZE-count,
				reg_list[i].fmt, val);
	}
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_object_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u8 addr, val;
	int error;

	mutex_lock(&ft5x06->mutex);
	if (sscanf(buf, "%hhx=%hhx", &addr, &val) == 2)
		error = ft5x06_write_byte(ft5x06, addr, val);
	else
		error = -EINVAL;
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_dbgdump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int count;

	mutex_lock(&ft5x06->mutex);
	count = sprintf(buf, "%d\n", ft5x06->dbgdump);
	mutex_unlock(&ft5x06->mutex);

	return count;
}

static ssize_t ft5x06_dbgdump_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	unsigned long dbgdump;
	int error;

	mutex_lock(&ft5x06->mutex);
	error = strict_strtoul(buf, 0, &dbgdump);
	if (!error)
		ft5x06->dbgdump = dbgdump;
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_updatefw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	struct ft5x06_firmware_data firmware;
	const struct firmware *fw;
	bool upgraded;
	int error;

	error = request_firmware(&fw, "ft5x06.bin", dev);
	if (!error) {
		firmware.data = fw->data;
		firmware.size = fw->size;

		mutex_lock(&ft5x06->mutex);
		error = ft5x06_load_firmware(ft5x06, &firmware, &upgraded);
		mutex_unlock(&ft5x06->mutex);

		release_firmware(fw);
	}

	return error ? : count;
}

static ssize_t ft5x06_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	int error;
	mutex_lock(&ft5x06->mutex);
	error = ft5x06_read_byte(ft5x06, FT5x0x_REG_FW_VER, &fwver);
	if (error)
		num_read_chars = snprintf(buf, PAGE_SIZE, "Get firmware version failed!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

static int ft5x06_enter_factory(struct ft5x06_data *ft5x06_ts)
{
	u8 reg_val;
	int error;

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return -1;
	msleep(100);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -1;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_TEST) {
		dev_info(ft5x06_ts->dev, "ERROR: The Touch Panel was not put in Factory Mode.");
		return -1;
	}

	return 0;
}

static int ft5x06_enter_work(struct ft5x06_data *ft5x06_ts)
{
	u8 reg_val;
	int error;
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return -1;
	msleep(100);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -1;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_NORMAL) {
		dev_info(ft5x06_ts->dev, "ERROR: The Touch Panel was not put in Normal Mode.\n");
		return -1;
	}

	return 0;
}

#define FT5x0x_TX_NUM		28
#define FT5x0x_RX_NUM   	16
static int ft5x06_get_rawData(struct ft5x06_data *ft5x06_ts,
					 u16 rawdata[][FT5x0x_RX_NUM])
{
	int ret_val = 0;
	int error;
	u8 val;
	int row_num = 0;
	u8 read_buffer[FT5x0x_RX_NUM * 2];
	int read_len;
	int i;

	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	val |= 0x80;
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Write mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	msleep(20);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	if (0x00 != (val & 0x80)) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	dev_info(ft5x06_ts->dev, "Read rawdata......\n");
	for (row_num = 0; row_num < FT5x0x_TX_NUM; row_num++) {
		memset(read_buffer, 0x00, (FT5x0x_RX_NUM * 2));
		error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_ROW_ADDR, row_num);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Write row addr failed!\n");
			ret_val = -1;
			goto error_return;
		}
		msleep(1);
		read_len = FT5x0x_RX_NUM * 2;
		error = ft5x06_write_byte(ft5x06_ts, 0x10, read_len);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Write len failed!\n");
			ret_val = -1;
			goto error_return;
		}
		error = ft5x06_read_block(ft5x06_ts, 0x10,
							read_buffer, FT5x0x_RX_NUM * 2);
		if (error < 0) {
			dev_err(ft5x06_ts->dev,
				"ERROR: Coule not read row %u data!\n", row_num);
			ret_val = -1;
			goto error_return;
		}
		for (i = 0; i < FT5x0x_RX_NUM; i++) {
			rawdata[row_num][i] = read_buffer[i<<1];
			rawdata[row_num][i] = rawdata[row_num][i] << 8;
			rawdata[row_num][i] |= read_buffer[(i<<1)+1];
		}
	}
error_return:
	return ret_val;
}

static int ft5x06_get_diffData(struct ft5x06_data *ft5x06_ts,
					 u16 diffdata[][FT5x0x_RX_NUM],
					 u16 *average)
{
	u16 after_rawdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int ret_val = 0;
	u8 reg_value;
	u8 orig_vol = 0;
	int i, j;
	unsigned int total = 0;
	struct ft5x06_ts_platform_data *pdata = ft5x06_ts->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	/*get original voltage and change it to get new frame rawdata*/
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, &reg_value);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not get voltage data!\n");
		goto error_return;
	} else
		orig_vol = reg_value;

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, 0);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not set voltage data to 0!\n");
		goto error_return;
	}

	for (i = 0; i < 3; i++) {
		error = ft5x06_get_rawData(ft5x06_ts, diffdata);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Could not get original raw data!\n");
			ret_val = error;
			goto error_return;
		}
	}

	reg_value = 2;

	dev_info(ft5x06_ts->dev, "original voltage: 0 changed voltage:%u\n",
		reg_value);

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, reg_value);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not set voltage data!\n");
		ret_val = error;
		goto error_return;
	}

	/* get raw data */
	for (i = 0; i < 3; i++) {
		error = ft5x06_get_rawData(ft5x06_ts, after_rawdata);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Could not get after raw data!\n");
			ret_val = error;
			goto error_voltage;
		}
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (after_rawdata[i][j] > diffdata[i][j])
				diffdata[i][j] = after_rawdata[i][j] - diffdata[i][j];
			else
				diffdata[i][j] = diffdata[i][j] - after_rawdata[i][j];

				total += diffdata[i][j];

			printk(KERN_CONT "%d ", diffdata[i][j]);
		}
		pr_info("total = %d\n", total);
	}

	*average = (u16)(total / (tx_num * rx_num));

error_voltage:
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, orig_vol);
	if (error < 0) {
		ret_val = error;
		dev_err(ft5x06_ts->dev, "ERROR: Could not get voltage data!\n");
	}

error_return:
	return ret_val;

}

static ssize_t ft5x06_rawdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u16	rawdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int i = 0, j = 0;
	int num_read_chars = 0;
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	mutex_lock(&ft5x06->mutex);

	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	error = ft5x06_get_rawData(ft5x06, rawdata);
	if (error < 0)
		sprintf(buf, "%s", "Could not get rawdata\n");
	else {
		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				num_read_chars += sprintf(&buf[num_read_chars],
								"%u ", rawdata[i][j]);
			}
			buf[num_read_chars-1] = '\n';
		}
	}

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

static ssize_t ft5x06_diffdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u16	diffdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int i = 0, j = 0;
	int num_read_chars = 0;
	u16 average;
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	mutex_lock(&ft5x06->mutex);
	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	error = ft5x06_get_diffData(ft5x06, diffdata, &average);
	if (error < 0)
		sprintf(buf, "%s", "Could not get rawdata\n");
	else {
		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				num_read_chars += sprintf(&buf[num_read_chars],
								"%u ", diffdata[i][j]);
			}
			buf[num_read_chars-1] = '\n';
		}
	}

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

unsigned int ft5x06_do_selftest(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	u16 testdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int i, j;
	int error;
	const struct ft5x06_keypad_data *keypad = pdata->keypad;
	u16 average;

	/* 1. test raw data */
	error = ft5x06_get_rawData(ft5x06, testdata);
	if (error)
		return 0;

	for (i = 0; i < pdata->tx_num; i++) {
		if (i != pdata->tx_num - 1)  {
			for (j = 0; j < pdata->rx_num; j++) {
				if (testdata[i][j] < pdata->raw_min ||
					testdata[i][j] > pdata->raw_max) {
						return 0;
					}
			}
		} else {
			for (j = 0; j < keypad->length; j++) {
				if (testdata[i][keypad->key_pos[j]] < pdata->raw_min ||
					testdata[i][keypad->key_pos[j]]  > pdata->raw_max) {
					return 0;
				}
			}
		}
	}

	/* 2. test diff data */
	error = ft5x06_get_diffData(ft5x06, testdata, &average);
	if (error)
		return 0;
	for (i = 0; i < pdata->tx_num - 1; i++) {
		for (j = 0; j < pdata->rx_num; j++) {
			if ((testdata[i][j] < average * 13 / 20) ||
				(testdata[i][j] > average * 27 / 20)) {
				dev_info(ft5x06->dev, "Failed, testdata = %d, average = %d\n",
					testdata[i][j], average);
				return 0;
			}
		}
	}

	return 1;
}

static ssize_t ft5x06_selftest_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int error;
	unsigned long val;

	error = strict_strtoul(buf, 0, &val);
	if (error )
		return error;
	if (val != 1)
		return -EINVAL;

	mutex_lock(&ft5x06->mutex);

	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	ft5x06->test_result = ft5x06_do_selftest(ft5x06);

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return count;
}

static ssize_t ft5x06_selftest_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);

	return sprintf(&buf[0], "%u\n", ft5x06->test_result);
}

/* sysfs */
static DEVICE_ATTR(tpfwver, 0644, ft5x06_tpfwver_show, NULL);
static DEVICE_ATTR(object, 0644, ft5x06_object_show, ft5x06_object_store);
static DEVICE_ATTR(dbgdump, 0644, ft5x06_dbgdump_show, ft5x06_dbgdump_store);
static DEVICE_ATTR(updatefw, 0200, NULL, ft5x06_updatefw_store);
static DEVICE_ATTR(rawdatashow, 0644, ft5x06_rawdata_show, NULL);
static DEVICE_ATTR(diffdatashow, 0644, ft5x06_diffdata_show, NULL);
static DEVICE_ATTR(selftest, 0644, ft5x06_selftest_show, ft5x06_selftest_store);

static struct attribute *ft5x06_attrs[] = {
	&dev_attr_tpfwver.attr,
	&dev_attr_object.attr,
	&dev_attr_dbgdump.attr,
	&dev_attr_updatefw.attr,
	&dev_attr_rawdatashow.attr,
	&dev_attr_diffdatashow.attr,
	&dev_attr_selftest.attr,
	NULL
};

static const struct attribute_group ft5x06_attr_group = {
	.attrs = ft5x06_attrs
};

struct ft5x06_data *ft5x06_probe(struct device *dev,
				const struct ft5x06_bus_ops *bops)
{
	int error;
	struct ft5x06_data *ft5x06;
	struct ft5x06_ts_platform_data *pdata;

	/* check input argument */
	pdata = dev->platform_data;
	if (pdata == NULL) {
		dev_err(dev, "platform data doesn't exist\n");
		error = -EINVAL;
		goto err;
	}

	/* init platform stuff */
	if (pdata->power_init) {
		error = pdata->power_init(true);
		if (error) {
			dev_err(dev, "fail to power_init platform\n");
			goto err;
		}
	}

	if (pdata->power_on) {
		error = pdata->power_on(true);
		if (error) {
			dev_err(dev, "fail to power on!\n");
			goto err_power_init;
		}
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		error = gpio_request(pdata->irq_gpio, "ft5x06_irq_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto err_power;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		error = gpio_request(pdata->reset_gpio, "ft5x06_reset_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto free_irq_gpio;
		}
		error = gpio_direction_output(pdata->reset_gpio, 1);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_reset_gpio;
		}
	}

	/* alloc and init data object */
	ft5x06 = kzalloc(sizeof(struct ft5x06_data), GFP_KERNEL);
	if (ft5x06 == NULL) {
		dev_err(dev, "fail to allocate data object\n");
		error = -ENOMEM;
		goto free_reset_gpio;
	}

	mutex_init(&ft5x06->mutex);

	ft5x06->dev  = dev;
	ft5x06->irq  = gpio_to_irq(pdata->irq_gpio);
	ft5x06->bops = bops;

	/* alloc and init input device */
	ft5x06->input = input_allocate_device();
	if (ft5x06->input == NULL) {
		dev_err(dev, "fail to allocate input device\n");
		error = -ENOMEM;
		goto err_free_data;
	}

	input_set_drvdata(ft5x06->input, ft5x06);
	ft5x06->input->name       = "ft5x06";
	ft5x06->input->id.bustype = bops->bustype;
	ft5x06->input->id.vendor  = 0x4654; /* FocalTech */
	ft5x06->input->id.product = 0x5000; /* ft5x0x    */
	ft5x06->input->id.version = 0x0100; /* 1.0       */
	ft5x06->input->dev.parent = dev;

	/* init touch parameter */
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	input_mt_init_slots(ft5x06->input, FT5X0X_MAX_FINGER);
#endif
	set_bit(ABS_MT_TOUCH_MAJOR, ft5x06->input->absbit);
	set_bit(ABS_MT_POSITION_X, ft5x06->input->absbit);
	set_bit(ABS_MT_POSITION_Y, ft5x06->input->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, ft5x06->input->absbit);
	set_bit(INPUT_PROP_DIRECT, ft5x06->input->propbit);

	input_set_abs_params(ft5x06->input,
			     ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_TOUCH_MAJOR, 0, pdata->z_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_WIDTH_MAJOR, 0, pdata->w_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	set_bit(EV_KEY, ft5x06->input->evbit);
	set_bit(EV_ABS, ft5x06->input->evbit);

	error = ft5x06_load_firmware(ft5x06, pdata->firmware, NULL);
	if (error) {
		dev_err(dev, "fail to load firmware\n");
		goto err_free_input;
	}

	/* register input device */
	error = input_register_device(ft5x06->input);
	if (error) {
		dev_err(dev, "fail to register input device\n");
		goto err_free_input;
	}

	ft5x06->input->phys =
		kobject_get_path(&ft5x06->input->dev.kobj, GFP_KERNEL);
	if (ft5x06->input->phys == NULL) {
		dev_err(dev, "fail to get input device path\n");
		error = -ENOMEM;
		goto err_unregister_input;
	}

	/* start interrupt process */
	error = request_threaded_irq(ft5x06->irq, NULL, ft5x06_interrupt,
				IRQF_TRIGGER_FALLING, "ft5x06", ft5x06);
	if (error) {
		dev_err(dev, "fail to request interrupt\n");
		goto err_free_phys;
	}

	/* export sysfs entries */
	ft5x06->vkeys_dir = kobject_create_and_add("board_properties", NULL);
	if (ft5x06->vkeys_dir == NULL) {
		error = -ENOMEM;
		dev_err(dev, "fail to create board_properties entry\n");
		goto err_free_irq;
	}

	sysfs_attr_init(&ft5x06->vkeys_attr.attr);
	ft5x06->vkeys_attr.attr.name = "virtualkeys.ft5x06";
	ft5x06->vkeys_attr.attr.mode = (S_IRUSR|S_IRGRP|S_IROTH);
	ft5x06->vkeys_attr.show      = ft5x06_vkeys_show;

	error = sysfs_create_file(ft5x06->vkeys_dir, &ft5x06->vkeys_attr.attr);
	if (error) {
		dev_err(dev, "fail to create virtualkeys entry\n");
		goto err_put_vkeys;
	}

	error = sysfs_create_group(&dev->kobj, &ft5x06_attr_group);
	if (error) {
		dev_err(dev, "fail to export sysfs entires\n");
		goto err_put_vkeys;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ft5x06->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
	ft5x06->early_suspend.suspend = ft5x06_early_suspend;
	ft5x06->early_suspend.resume  = ft5x06_early_resume;
	register_early_suspend(&ft5x06->early_suspend);
#endif

	ft5x06->power_supply_notifier.notifier_call = ft5x06_power_supply_event;
	register_power_supply_notifier(&ft5x06->power_supply_notifier);

	INIT_DELAYED_WORK(&ft5x06->noise_filter_delayed_work,
				ft5x06_noise_filter_delayed_work);
	return ft5x06;

err_put_vkeys:
	kobject_put(ft5x06->vkeys_dir);
err_free_irq:
	free_irq(ft5x06->irq, ft5x06);
err_free_phys:
	kfree(ft5x06->input->phys);
err_unregister_input:
	input_unregister_device(ft5x06->input);
	ft5x06->input = NULL;
err_free_input:
	input_free_device(ft5x06->input);
err_free_data:
	kfree(ft5x06);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_power:
	if (pdata->power_on)
		pdata->power_on(false);
err_power_init:
	if (pdata->power_init)
		pdata->power_init(false);
err:
	return ERR_PTR(error);
}
EXPORT_SYMBOL_GPL(ft5x06_probe);

void ft5x06_remove(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;

	cancel_delayed_work_sync(&ft5x06->noise_filter_delayed_work);
	unregister_power_supply_notifier(&ft5x06->power_supply_notifier);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ft5x06->early_suspend);
#endif
	sysfs_remove_group(&ft5x06->dev->kobj, &ft5x06_attr_group);
	kobject_put(ft5x06->vkeys_dir);
	free_irq(ft5x06->irq, ft5x06);
	kfree(ft5x06->input->phys);
	input_unregister_device(ft5x06->input);
	kfree(ft5x06);
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (pdata->power_on)
		pdata->power_on(false);
	if (pdata->power_init)
		pdata->power_init(false);
}
EXPORT_SYMBOL_GPL(ft5x06_remove);

MODULE_AUTHOR("Zhang Bo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("ft5x0x touchscreen input driver");
MODULE_LICENSE("GPL");
