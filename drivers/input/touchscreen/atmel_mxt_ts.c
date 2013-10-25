/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define MXT_SUSPEND_LEVEL 1
#endif

/* Family ID */
#define MXT224_ID	0x80
#define MXT224E_ID	0x81
#define MXT336S_ID	0x82
#define MXT384E_ID	0xA1
#define MXT1386_ID	0xA0

/* Version */
#define MXT_VER_20		20
#define MXT_VER_21		21
#define MXT_VER_22		22

/* I2C slave address pairs */
struct mxt_address_pair {
	int bootloader;
	int application;
};

static const struct mxt_address_pair mxt_slave_addresses[] = {
	{ 0x24, 0x4a },
	{ 0x25, 0x4b },
	{ 0x26, 0x4c },
	{ 0x27, 0x4d },
	{ 0x34, 0x5a },
	{ 0x35, 0x5b },
	{ 0 },
};

enum mxt_device_state { INIT, APPMODE, BOOTLOADER };

/* Firmware */
#define MXT_FW_NAME		"maxtouch.fw"

/* Firmware frame size including frame data and CRC */
#define MXT_SINGLE_FW_MAX_FRAME_SIZE	278
#define MXT_CHIPSET_FW_MAX_FRAME_SIZE	534

/* Registers */
#define MXT_FAMILY_ID		0x00
#define MXT_VARIANT_ID		0x01
#define MXT_VERSION		0x02
#define MXT_BUILD		0x03
#define MXT_MATRIX_X_SIZE	0x04
#define MXT_MATRIX_Y_SIZE	0x05
#define MXT_OBJECT_NUM		0x06
#define MXT_OBJECT_START	0x07

#define MXT_OBJECT_SIZE		6

#define MXT_MAX_BLOCK_WRITE	256

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYADDTHD_T14		14
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCI_ADAPTIVETHRESHOLD_T55 55
#define MXT_PROCI_SHIELDLESS_T56	56
#define MXT_PROCI_EXTRATOUCHDATA_T57	57
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_PROCG_NOISESUPPRESSION_T62	62
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_SPT_TIMER_T61		61
#define MXT_PROCI_LENSBENDING_T65	65
#define MXT_SPT_GOLDENREF_T66		66
#define MXT_SPT_DYMCFG_T70		70
#define MXT_SPT_DYMDATA_T71		71
#define MXT_PROCG_NOISESUPPRESSION_T72	72

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* MXT_GEN_POWER_T7 field */
#define MXT_POWER_IDLEACQINT	0
#define MXT_POWER_ACTVACQINT	1
#define MXT_POWER_ACTV2IDLETO	2

/* MXT_GEN_ACQUIRE_T8 field */
#define MXT_ACQUIRE_CHRGTIME	0
#define MXT_ACQUIRE_TCHDRIFT	2
#define MXT_ACQUIRE_DRIFTST	3
#define MXT_ACQUIRE_TCHAUTOCAL	4
#define MXT_ACQUIRE_SYNC	5
#define MXT_ACQUIRE_ATCHCALST	6
#define MXT_ACQUIRE_ATCHCALSTHR	7
#define MXT_ACQUIRE_ATCHFRCCALTHR	8
#define MXT_ACQUIRE_ATCHFRCCALRATIO	9

/* MXT_TOUCH_MULT_T9 field */
#define MXT_TOUCH_CTRL		0
#define MXT_TOUCH_XORIGIN	1
#define MXT_TOUCH_YORIGIN	2
#define MXT_TOUCH_XSIZE		3
#define MXT_TOUCH_YSIZE		4
#define MXT_TOUCH_BLEN		6
#define MXT_TOUCH_TCHTHR	7
#define MXT_TOUCH_TCHDI		8
#define MXT_TOUCH_ORIENT	9
#define MXT_TOUCH_MOVHYSTI	11
#define MXT_TOUCH_MOVHYSTN	12
#define MXT_TOUCH_NUMTOUCH	14
#define MXT_TOUCH_MRGHYST	15
#define MXT_TOUCH_MRGTHR	16
#define MXT_TOUCH_AMPHYST	17
#define MXT_TOUCH_XRANGE_LSB	18
#define MXT_TOUCH_XRANGE_MSB	19
#define MXT_TOUCH_YRANGE_LSB	20
#define MXT_TOUCH_YRANGE_MSB	21
#define MXT_TOUCH_XLOCLIP	22
#define MXT_TOUCH_XHICLIP	23
#define MXT_TOUCH_YLOCLIP	24
#define MXT_TOUCH_YHICLIP	25
#define MXT_TOUCH_XEDGECTRL	26
#define MXT_TOUCH_XEDGEDIST	27
#define MXT_TOUCH_YEDGECTRL	28
#define MXT_TOUCH_YEDGEDIST	29
#define MXT_TOUCH_JUMPLIMIT	30

/* MXT_TOUCH_KEYARRAY_T15 field */
#define MXT_KEYARRAY_XORIGIN	1
#define MXT_KEYARRAY_YORIGIN	2
#define MXT_KEYARRAY_GAIN	6
#define MXT_KEYARRAY_THRESHOLD	7

/* MXT_PROCI_GRIPFACE_T20 field */
#define MXT_GRIPFACE_CTRL	0
#define MXT_GRIPFACE_XLOGRIP	1
#define MXT_GRIPFACE_XHIGRIP	2
#define MXT_GRIPFACE_YLOGRIP	3
#define MXT_GRIPFACE_YHIGRIP	4
#define MXT_GRIPFACE_MAXTCHS	5
#define MXT_GRIPFACE_SZTHR1	7
#define MXT_GRIPFACE_SZTHR2	8
#define MXT_GRIPFACE_SHPTHR1	9
#define MXT_GRIPFACE_SHPTHR2	10
#define MXT_GRIPFACE_SUPEXTTO	11

/* MXT_PROCI_NOISE field */
#define MXT_NOISE_CTRL		0
#define MXT_NOISE_OUTFLEN	1
#define MXT_NOISE_GCAFUL_LSB	3
#define MXT_NOISE_GCAFUL_MSB	4
#define MXT_NOISE_GCAFLL_LSB	5
#define MXT_NOISE_GCAFLL_MSB	6
#define MXT_NOISE_ACTVGCAFVALID	7
#define MXT_NOISE_NOISETHR	8
#define MXT_NOISE_FREQHOPSCALE	10
#define MXT_NOISE_FREQ0		11
#define MXT_NOISE_FREQ1		12
#define MXT_NOISE_FREQ2		13
#define MXT_NOISE_FREQ3		14
#define MXT_NOISE_FREQ4		15
#define MXT_NOISE_IDLEGCAFVALID	16

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1

/* MXT_SPT_CTECONFIG_T28 field */
#define MXT_CTE_CTRL		0
#define MXT_CTE_CMD		1
#define MXT_CTE_MODE		2
#define MXT_CTE_IDLEGCAFDEPTH	3
#define MXT_CTE_ACTVGCAFDEPTH	4
#define MXT_CTE_VOLTAGE		5

/* MXT_DEBUG_DIAGNOSTIC_T37 */
#define MXT_DIAG_MODE	0
#define MXT_DIAG_PAGE	1
#define MXT_DIAG_DATA	2

/* MXT_SPT_USERDATA_T38 */
#define MXT_FW_UPDATE_FLAG	6

#define MXT_VOLTAGE_DEFAULT	2700000
#define MXT_VOLTAGE_STEP	10000

/* Analog voltage @2.7 V */
#define MXT_VTG_MIN_UV		2700000
#define MXT_VTG_MAX_UV		3300000
#define MXT_ACTIVE_LOAD_UA	15000
#define MXT_LPM_LOAD_UA		10
/* Digital voltage @1.8 V */
#define MXT_VTG_DIG_MIN_UV	1800000
#define MXT_VTG_DIG_MAX_UV	1800000
#define MXT_ACTIVE_LOAD_DIG_UA	10000
#define MXT_LPM_LOAD_DIG_UA	10

#define MXT_I2C_VTG_MIN_UV	1800000
#define MXT_I2C_VTG_MAX_UV	1800000
#define MXT_I2C_LOAD_UA		10000
#define MXT_I2C_LPM_LOAD_UA	10

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_BACKUP_VALUE	0x55
#define MXT_BACKUP_TIME		25	/* msec */
#define MXT224_RESET_TIME	65	/* msec */
#define MXT224E_RESET_TIME	150	/* msec */
#define MXT336S_RESET_TIME	25	/* msec */
#define MXT384E_RESET_TIME	196	/* msec */
#define MXT1386_RESET_TIME	250	/* msec */
#define MXT_RESET_TIME		250	/* msec */
#define MXT_RESET_NOCHGREAD	400	/* msec */

#define MXT_FWRESET_TIME	1000	/* msec */

#define MXT_WAKE_TIME		25

#define MXT_STOP_ANTIPALM_TIME               (5 * HZ)
#define MXT_STOP_ANTIPALM_TIMEOUT         (20 * HZ)
#define MXT_FORCE_CALIBRATE_DELAY          (HZ / 2)

#define MXT_PAGE_UP		0x01
#define MXT_DELTA_DATA		0x10
#define MXT_REFERENCE_DATA	0x11

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Touch status */
#define MXT_SUPPRESS		(1 << 1)
#define MXT_AMP			(1 << 2)
#define MXT_VECTOR		(1 << 3)
#define MXT_MOVE		(1 << 4)
#define MXT_RELEASE		(1 << 5)
#define MXT_PRESS		(1 << 6)
#define MXT_DETECT		(1 << 7)

/* Touch orient bits */
#define MXT_XY_SWITCH		(1 << 0)
#define MXT_X_INVERT		(1 << 1)
#define MXT_Y_INVERT		(1 << 2)

/* Touch suppression */
#define MXT_TCHSUP_ACTIVE	(1 << 0)

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

#define MXT_MAX_FINGER		16

#define T7_DATA_SIZE		3
#define T8_DATA_SIZE		10
#define MXT_MAX_RW_TRIES	3
#define MXT_BLOCK_SIZE		256
#define MXT_CFG_VERSION_LEN	3
#define MXT_CFG_VERSION_ARRAYSIZE	8
#define MXT_CFG_VERSION_EQUAL	0
#define MXT_CFG_VERSION_NOT_EQUAL	1

#define MXT_DEBUGFS_DIR		"atmel_mxt_ts"
#define MXT_DEBUGFS_FILE	"object"

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size;
	u8 instances;
	u8 num_report_ids;

	/* to map object and message */
	u8 max_reportid;
};

struct mxt_message {
	u8 reportid;
	u8 message[7];
	u8 checksum;
};

struct mxt_finger {
	int status;
	int x;
	int y;
	int area;
	int pressure;
};

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct mxt_platform_data *pdata;
	const struct mxt_config_info *config_info;
	enum mxt_device_state state;
	struct mxt_object *object_table;
	u16 mem_size;
	struct mxt_info info;
	struct mxt_finger finger[MXT_MAX_FINGER];
	unsigned long dbgdump;
	unsigned int irq;
	struct regulator *vcc_ana;
	struct regulator *vcc_dig;
	struct regulator *vcc_i2c;
	struct delayed_work force_calibrate_delayed_work;
	struct delayed_work disable_antipalm_delayed_work;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif

	u8 in_chip_crc[3];
	u8 t7_data[T7_DATA_SIZE];
	u8 t8_data[T8_DATA_SIZE];
	u8 t6_reportid;
	u16 t7_start_addr;
	u16 t8_start_addr;
	u32 keyarray_old;
	u32 keyarray_new;
	u8 t9_max_reportid;
	u8 t9_min_reportid;
	u8 t15_max_reportid;
	u8 t15_min_reportid;
	u8 t25_max_reportid;
	u8 t25_min_reportid;
	u16 t37_start_addr;
	u8 t37_object_size;
	u8 t42_max_reportid;
	u8 t42_min_reportid;
	u8 test_result[6];
	u8 cfg_version[MXT_CFG_VERSION_ARRAYSIZE];
	int cfg_version_idx;
	int t38_start_addr;
	bool update_cfg;
	const char *fw_name;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool driver_paused;
	bool disable_antipalm_done;
	int land_x;
	int land_y;
	int is_land_signed;
	int key_pressed_count;
	int lcd_id;
	bool is_crc_got;
	bool is_key_verify;
};

static struct dentry *debug_base;

static bool mxt_object_writable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYADDTHD_T14:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCI_ADAPTIVETHRESHOLD_T55:
	case MXT_PROCI_SHIELDLESS_T56:
	case MXT_PROCI_EXTRATOUCHDATA_T57:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_PROCG_NOISESUPPRESSION_T62:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
	case MXT_SPT_TIMER_T61:
	case MXT_PROCI_LENSBENDING_T65:
	case MXT_SPT_GOLDENREF_T66:
	case MXT_SPT_DYMCFG_T70:
	case MXT_SPT_DYMDATA_T71:
	case MXT_PROCG_NOISESUPPRESSION_T72:
		return true;
	default:
		return false;
	}
}

static int mxt_switch_to_bootloader_address(struct mxt_data *data)
{
	int i;
	struct i2c_client *client = data->client;

	if (data->state == BOOTLOADER) {
		dev_err(&client->dev, "Already in BOOTLOADER state\n");
		return -EINVAL;
	}

	for (i = 0; mxt_slave_addresses[i].application != 0;  i++) {
		if (mxt_slave_addresses[i].application == client->addr) {
			dev_info(&client->dev, "Changing to bootloader address: "
				"%02x -> %02x",
				client->addr,
				mxt_slave_addresses[i].bootloader);

			client->addr = mxt_slave_addresses[i].bootloader;
			data->state = BOOTLOADER;
			return 0;
		}
	}

	dev_err(&client->dev, "Address 0x%02x not found in address table",
								client->addr);
	return -EINVAL;
}

static int mxt_switch_to_appmode_address(struct mxt_data *data)
{
	int i;
	struct i2c_client *client = data->client;

	if (data->state == APPMODE) {
		dev_err(&client->dev, "Already in APPMODE state\n");
		return -EINVAL;
	}

	for (i = 0; mxt_slave_addresses[i].application != 0;  i++) {
		if (mxt_slave_addresses[i].bootloader == client->addr) {
			dev_info(&client->dev,
				"Changing to application mode address: "
							"0x%02x -> 0x%02x",
				client->addr,
				mxt_slave_addresses[i].application);

			client->addr = mxt_slave_addresses[i].application;
			data->state = APPMODE;
			return 0;
		}
	}

	dev_err(&client->dev, "Address 0x%02x not found in address table",
								client->addr);
	return -EINVAL;
}

static int mxt_get_bootloader_id(struct i2c_client *client)
{
	u8 val;
	u8 buf[3];

	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	if (val & MXT_BOOT_EXTENDED_ID)	{
		if (i2c_master_recv(client, &buf[0], 3) != 3) {
			dev_err(&client->dev, "%s: i2c recv failed\n",
								__func__);
			return -EIO;
		}
		return buf[1];
	} else {
		dev_info(&client->dev, "Bootloader ID:%d",
			val & MXT_BOOT_ID_MASK);

		return val & MXT_BOOT_ID_MASK;
	}
}

static int mxt_check_bootloader(struct i2c_client *client,
				unsigned int state)
{
	u8 val;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK)
			goto recheck;
		if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(&client->dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(&client->dev, "Invalid bootloader mode state %X\n",
			val);
		return -EINVAL;
	}

	return 0;
}

static int mxt_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2];

	buf[0] = MXT_UNLOCK_CMD_LSB;
	buf[1] = MXT_UNLOCK_CMD_MSB;

	if (i2c_master_send(client, buf, 2) != 2) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt_fw_write(struct i2c_client *client,
			const u8 *data, unsigned int frame_size)
{
	if (i2c_master_send(client, data, frame_size) != frame_size) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int __mxt_read_reg_no_retry(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	if (i2c_transfer(client->adapter, xfer, 2) == 2)
		return 0;

	dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
	return -EIO;
}

static int __mxt_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	int i = 0;
	int error;

	do {
		error = __mxt_read_reg_no_retry(client, reg, len, val);
		if (error == 0)
			return 0;
		msleep(MXT_WAKE_TIME);
	} while (++i < MXT_MAX_RW_TRIES);

	return error;
}

static int mxt_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	return __mxt_read_reg(client, reg, 1, val);
}

static int __mxt_write_reg(struct i2c_client *client,
		    u16 addr, u16 length, u8 *value)
{
	u8 buf[MXT_BLOCK_SIZE + 2];
	int i, tries = 0;

	if (length > MXT_BLOCK_SIZE)
		return -EINVAL;

	buf[0] = addr & 0xff;
	buf[1] = (addr >> 8) & 0xff;
	for (i = 0; i < length; i++)
		buf[i + 2] = *value++;

	do {
		if (i2c_master_send(client, buf, length + 2) == (length + 2))
			return 0;
		msleep(MXT_WAKE_TIME);
	} while (++tries < MXT_MAX_RW_TRIES);

	dev_err(&client->dev, "%s: i2c send failed\n", __func__);
	return -EIO;
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static int mxt_read_object_table(struct i2c_client *client,
				      u16 reg, u8 *object_buf)
{
	return __mxt_read_reg(client, reg, MXT_OBJECT_SIZE,
				   object_buf);
}

static struct mxt_object *
mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type\n");
	return NULL;
}

static int mxt_read_message(struct mxt_data *data,
				 struct mxt_message *message)
{
	struct mxt_object *object;
	u16 reg;
	int ret;

	object = mxt_get_object(data, MXT_GEN_MESSAGE_T5);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	ret = __mxt_read_reg(data->client, reg,
			     sizeof(struct mxt_message), message);

	if (ret == 0 && message->reportid != MXT_RPTID_NOMSG
	    && data->debug_enabled)
		print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
			       message, sizeof(struct mxt_message), false);

	return ret;
}

static int mxt_read_object(struct mxt_data *data,
				u8 type, u8 offset, u8 *val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return __mxt_read_reg(data->client, reg + offset, 1, val);
}

static int mxt_write_object(struct mxt_data *data,
				 u8 type, u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return mxt_write_reg(data->client, reg + offset, val);
}

#define FULL_OFF_STATE		0
#define FULL_ON_STATE		1
#define NORMAL_STATE		2

static void mxt_anti_palm_control(struct mxt_data *data, u8 flag)
{
	int error;
	u8* anti_palm_value = data->t8_data + MXT_ACQUIRE_ATCHFRCCALTHR;
	u8 value[2];

	value[0] = anti_palm_value[0];
	value[1] = anti_palm_value[1];

	if (FULL_OFF_STATE == flag) {
		value[0] = 0;
		value[1] = 0;
	}

	/* restore the saved anti-palm setting */
	error = __mxt_write_reg(data->client,
				data->t8_start_addr + MXT_ACQUIRE_ATCHFRCCALTHR,
				2, value);
	if (error < 0)
		dev_err(&data->client->dev,
			"failed to restore saved anti-palm setting\n");

	if (NORMAL_STATE == flag)
		dev_info(&data->client->dev,
				"Set anti-palm to normal state.\n");
	else if (FULL_OFF_STATE == flag)
		dev_info(&data->client->dev,
				"Set anti-palm to full-off state.\n");
}

static void mxt_disable_antipalm_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct mxt_data *data = container_of(delayed_work, struct mxt_data, disable_antipalm_delayed_work);

	if (data->dbgdump >= 1)
		dev_info(&data->client->dev, "disable anti-palm setting\n");

	mxt_anti_palm_control(data, FULL_OFF_STATE);
}

static void mxt_clear_touch_event(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	struct mxt_finger *finger = data->finger;
	int id;
	static int tracking_id = -2;

	for (id = 0; id < MXT_MAX_FINGER; id++) {
		finger[id].status = MXT_RELEASE;
		input_mt_slot(input_dev, id);
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, tracking_id--);
		if (tracking_id >= 0)
			tracking_id = -2;
	}

	input_sync(input_dev);
}

static void mxt_input_report(struct mxt_data *data, int single_id)
{
	struct mxt_finger *finger = data->finger;
	struct input_dev *input_dev = data->input_dev;
	int status = finger[single_id].status;
	int finger_num = 0;
	int id;

	for (id = 0; id < MXT_MAX_FINGER; id++) {
		if (!finger[id].status)
			continue;

		input_mt_slot(input_dev, id);
		/* Firmware reports min/max values when the touch is
		 * outside screen area. Send a release event in
		 * such cases to avoid unwanted touches.
		 */
		if (finger[id].x <= data->pdata->panel_minx ||
				finger[id].x >= data->pdata->panel_maxx ||
				finger[id].y <= data->pdata->panel_miny ||
				finger[id].y >= data->pdata->panel_maxy) {
			finger[id].status = MXT_RELEASE;
		}

		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
				finger[id].status != MXT_RELEASE);

		if (finger[id].status != MXT_RELEASE) {
			finger_num++;
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					finger[id].area);
			input_report_abs(input_dev, ABS_MT_POSITION_X,
					finger[id].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y,
					finger[id].y);
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 finger[id].pressure);
			if (!data->is_land_signed) {
				data->land_x = finger[id].x;
				data->land_y = finger[id].y;
				data->is_land_signed = 1;
			}

		} else {
			finger[id].status = 0;
		}
	}

	if (finger_num == 0 &&
		!data->disable_antipalm_done &&
		data->is_land_signed) {
		/* disable both anti-touch and anti-palm */
		int delta_x = finger[single_id].x - data->land_x;
		int delta_y = finger[single_id].y - data->land_y;
		if(delta_x * delta_x +  delta_y * delta_y >=  data->pdata->move_threshold) {
			dev_info(&data->client->dev, "Unlocked, shedule close anti calibration work!\n");
			cancel_delayed_work(&data->disable_antipalm_delayed_work);
			schedule_delayed_work(&data->disable_antipalm_delayed_work, MXT_STOP_ANTIPALM_TIME);
			data->disable_antipalm_done = true;
		} else {
			data->is_land_signed = 0;
		}
	} else if (finger_num != 1)
		data->is_land_signed = 0;

	if (finger[single_id].x <= data->pdata->panel_minx ||
		finger[single_id].x >= data->pdata->panel_maxx ||
		finger[single_id].y <= data->pdata->panel_miny ||
		finger[single_id].y >= data->pdata->panel_maxy) {
		status = MXT_RELEASE;
	}

	input_sync(input_dev);
}

static void mxt_input_touchevent(struct mxt_data *data,
				      struct mxt_message *message, int id)
{
	struct mxt_finger *finger = data->finger;
	struct device *dev = &data->client->dev;
	u8 status = message->message[0];
	int x;
	int y;
	int area;
	int pressure;

	x = (message->message[1] << 4) | ((message->message[3] >> 4) & 0xf);
	y = (message->message[2] << 4) | ((message->message[3] & 0xf));
	if (data->pdata->panel_maxx < 1024)
		x = x >> 2;
	if (data->pdata->panel_maxy < 1024)
		y = y >> 2;

	area = message->message[4];
	pressure = message->message[5];

	/* Check the touch is present on the screen */
	if (!(status & MXT_DETECT)) {
		if (status & MXT_RELEASE) {
			if (data->dbgdump >= 2) {
				dev_info(dev, "[%d] released x: %d, y: %d, area: %d,"
					" pressure: %d\n", id, x, y, area, pressure);
			}
			finger[id].status = MXT_RELEASE;
			mxt_input_report(data, id);
		}
		return;
	}

	/* Check only AMP detection */
	if (!(status & (MXT_PRESS | MXT_MOVE)))
		return;

	if (data->dbgdump >= 2) {
		dev_info(dev, "[%d] %s x: %d, y: %d, area: %d, pressure: %d\n",
			id, status & MXT_MOVE ? "moved" : "pressed",
			x, y, area, pressure);
	}

	finger[id].status = status & MXT_MOVE ?
				MXT_MOVE : MXT_PRESS;
	finger[id].x = x;
	finger[id].y = y;
	finger[id].area = area;
	finger[id].pressure = pressure;

	mxt_input_report(data, id);
}

static void mxt_handle_key_array(struct mxt_data *data,
				struct mxt_message *message)
{
	u32 keys_changed;
	int i;

	if (!data->pdata->key_codes) {
		dev_err(&data->client->dev, "keyarray is not supported\n");
		return;
	}

	data->keyarray_new = message->message[1] |
				(message->message[2] << 8) |
				(message->message[3] << 16) |
				(message->message[4] << 24);

	keys_changed = data->keyarray_old ^ data->keyarray_new;

	if (!keys_changed) {
		if (data->dbgdump >= 2)
			dev_info(&data->client->dev, "no keys changed\n");
		return;
	}

	for (i = 0; i < MXT_KEYARRAY_MAX_KEYS; i++) {
		if (!(keys_changed & (1 << i)))
			continue;

		if (data->dbgdump >= 2) {
			dev_info(&data->client->dev,
				"[%d] code: %d, value: %d\n",
				 i, data->pdata->key_codes[i],
				 data->keyarray_new & (1 << i));
		}

		if (data->keyarray_new & (1 << i))
			data->key_pressed_count++;
		else
			data->key_pressed_count--;

		input_report_key(data->input_dev, data->pdata->key_codes[i],
					(data->keyarray_new & (1 << i)));
		input_sync(data->input_dev);
	}

	dev_info(&data->client->dev, "Key count = %d\n", data->key_pressed_count);

	data->keyarray_old = data->keyarray_new;
}

static void mxt_handle_selftest(struct mxt_data *data,
				struct mxt_message *message)
{
	memcpy(data->test_result,
		message->message, sizeof(data->test_result));
}

static void mxt_release_all(struct mxt_data *data)
{
	int id;

	for (id = 0; id < MXT_MAX_FINGER; id++)
		if (data->finger[id].status)
			data->finger[id].status = MXT_RELEASE;

	mxt_input_report(data, 0);
}

static void mxt_handle_touch_supression(struct mxt_data *data, u8 status)
{
	if (data->dbgdump >= 1)
		dev_info(&data->client->dev, "touch suppression\n");

	/* release all touches */
	if (status & MXT_TCHSUP_ACTIVE)
		mxt_release_all(data);
}

#define MAX_KEY_NUM	3

static int mxt_do_diagnostic(struct mxt_data *data, u8 mode)
{
	int error = 0;
	u8 val;
	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, mode);
	if (error) {
		dev_err(&data->client->dev, "Failed to diag ref data value\n");
		return error;
	}

	while(1) {
		error = mxt_read_object(data, MXT_GEN_COMMAND_T6,
				MXT_COMMAND_DIAGNOSTIC, &val);
		if (error) {
			dev_err(&data->client->dev, "Failed to diag ref data value\n");
			 return error;
		}
		if (val == 0)
			break;
	}

	return error;
}

static void mxt_adjust_key_setting(struct mxt_data *data)
{
	int error = 0;
	/* enable t14 */
	error = mxt_write_object(data, MXT_TOUCH_KEYADDTHD_T14,
				0x0, 0x1);
	if (error) {
		dev_err(&data->client->dev, "Failed to disable T14\n");
		return;
	}

	error = mxt_write_object(data, MXT_TOUCH_KEYARRAY_T15,
				MXT_KEYARRAY_GAIN, data->config_info->key_gain);
	if (error) {
		dev_err(&data->client->dev, "Failed to set t15 gain\n");
		return;
	}

	error = mxt_write_object(data, MXT_TOUCH_KEYARRAY_T15,
				MXT_KEYARRAY_THRESHOLD, data->config_info->key_threshold);
	if (error) {
		dev_err(&data->client->dev, "Failed to set t15 threshold\n");
		return;
	}
}

static void mxt_handle_extra_touchdata(struct mxt_data *data, u8 mode)
{
	int error = 0;
	u8 orix, oriy;
	int count = 0;
	int offset = 0;
	int i;
	u8 buf[2];
	bool is_overflow = false;


	/* check t37 for button status */
	error = mxt_do_diagnostic(data, mode);
	if (error) {
		goto end;
	}

	error = mxt_read_object(data, MXT_TOUCH_KEYARRAY_T15,
			MXT_KEYARRAY_XORIGIN, &orix);
	if (error) {
		dev_err(&data->client->dev, "Failed to read x_origin value\n");
		goto end;
	}

	error = mxt_read_object(data, MXT_TOUCH_KEYARRAY_T15,
			MXT_KEYARRAY_YORIGIN, &oriy);
	if (error) {
		dev_err(&data->client->dev, "Failed to read y_origin value\n");
		goto end;
	}

	count = data->t37_object_size;
	while (count < orix * data->info.matrix_ysize * 2) {
		// if not arrived at the button position, page up
		error = mxt_do_diagnostic(data, MXT_PAGE_UP);
		if (error) {
			goto end;
		}

		count += data->t37_object_size;
	}

	count -= data->t37_object_size;
	offset =  2 + (orix * data->info.matrix_ysize * 2 -count) + (oriy * 2);

	for (i = 0; i < MAX_KEY_NUM; i++) {
		short value = 0;
		/* if offset larger than t37 object size, read one more page */
		if (offset > data->t37_object_size)
		{
			error = mxt_do_diagnostic(data, MXT_PAGE_UP);
			if (error) {
				goto end;
			}
			offset = (offset - data->t37_object_size);
		}

		error = __mxt_read_reg(data->client, data->t37_start_addr + offset,
				2, buf);
		if (error) {
			dev_err(&data->client->dev, "read key cpacitance delta faied!\n");
			goto end;
		}

		value = (buf[1] << 8) | (buf[0]);
		if (mode ==  MXT_DELTA_DATA)
			dev_info(&data->client->dev, "delta = %d\n", value);
		else if (mode == MXT_REFERENCE_DATA) {
			if (value > data->pdata->max_ref)
				is_overflow = true;
			dev_info(&data->client->dev, "ref = 0x%x\n", value);
		}
		offset += oriy*2 + 2;
	}

	if (data->config_info->key_gain != 0) {
		if (is_overflow && !data->is_key_verify) {
			mxt_adjust_key_setting(data);
			data->is_key_verify = true;
		}
	}
end:
	return;
}

#define MXT_COMMAND_CHECKSUM		1

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;
	struct mxt_message message;
	struct device *dev = &data->client->dev;
	int id;
	u8 reportid;

	if (data->state != APPMODE) {
		dev_err(dev, "Ignoring IRQ - not in APPMODE state\n");
		return IRQ_HANDLED;
	}

	do {
		if (mxt_read_message(data, &message)) {
			dev_err(dev, "Failed to read message\n");
			goto end;
		}
		reportid = message.reportid;

		if (!reportid) {
			dev_dbg(dev, "Report id 0 is reserved\n");
			continue;
		}

		if (data->driver_paused) {
			dev_dbg(dev, "Driver is paused\n");
			continue;
		}

		if (reportid == data->t6_reportid && !data->is_crc_got) {
			/* save config CRC */
			memcpy(data->in_chip_crc,
				&message.message[MXT_COMMAND_CHECKSUM],
				sizeof(data->in_chip_crc));
			dev_info(dev, "In chip crc = 0x%x, 0x%x, 0x%x\n",
					data->in_chip_crc[0],
					data->in_chip_crc[1],
					data->in_chip_crc[2]);
			data->is_crc_got = true;
		}

		/* check whether report id is part of T9 or T15 */
		id = reportid - data->t9_min_reportid;

		if (reportid >= data->t9_min_reportid &&
					reportid <= data->t9_max_reportid)
			mxt_input_touchevent(data, &message, id);
		else if (reportid >= data->t15_min_reportid &&
					reportid <= data->t15_max_reportid)
			mxt_handle_key_array(data, &message);
		else if (reportid >= data->t25_min_reportid &&
					reportid <= data->t25_max_reportid)
			mxt_handle_selftest(data, &message);
		else if (reportid >= data->t42_min_reportid &&
					reportid <= data->t42_max_reportid)
			mxt_handle_touch_supression(data, message.message[0]);
	} while (reportid != MXT_RPTID_NOMSG);

end:
	return IRQ_HANDLED;
}

static void mxt_get_crc(struct mxt_data *data)
{
	mxt_interrupt(data->client->irq, data);
}

static int mxt_check_reg_init(struct mxt_data *data)
{
	const struct mxt_config_info *config_info = data->config_info;
	struct mxt_object *object;
	struct device *dev = &data->client->dev;
	int index = 0;
	int i, j, config_offset;

	if (!config_info) {
		dev_dbg(dev, "No cfg data defined, skipping reg init\n");
		return 0;
	}

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_writable(object->type))
			continue;

		for (j = 0;
		     j < (object->size + 1) * (object->instances + 1);
		     j++) {
			config_offset = index + j;
			if (config_offset > config_info->config_length) {
				dev_err(dev, "Not enough config data!\n");
				return -EINVAL;
			}
			mxt_write_object(data, object->type, j,
					 config_info->config[config_offset]);
		}
		index += (object->size + 1) * (object->instances + 1);
	}

	return 0;
}

static int mxt_make_highchg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_message message;
	int count = 10;
	int error;

	/* Read dummy message to make high CHG pin */
	do {
		error = mxt_read_message(data, &message);
		if (error)
			return error;
	} while (message.reportid != MXT_RPTID_NOMSG && --count);

	if (!count) {
		dev_err(dev, "CHG pin isn't cleared\n");
		return -EBUSY;
	}

	return 0;
}

static int mxt_get_info(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;
	u8 val;

	error = __mxt_read_reg_no_retry(client, MXT_FAMILY_ID, 1, &val);
	if (error)
		return error;
	info->family_id = val;

	error = mxt_read_reg(client, MXT_VARIANT_ID, &val);
	if (error)
		return error;
	info->variant_id = val;

	error = mxt_read_reg(client, MXT_VERSION, &val);
	if (error)
		return error;
	info->version = val;

	error = mxt_read_reg(client, MXT_BUILD, &val);
	if (error)
		return error;
	info->build = val;

	error = mxt_read_reg(client, MXT_OBJECT_NUM, &val);
	if (error)
		return error;
	info->object_num = val;

	return 0;
}

static int mxt_get_object_table(struct mxt_data *data)
{
	int error;
	int i;
	u16 reg;
	u16 end_address;
	u8 reportid = 0;
	u8 buf[MXT_OBJECT_SIZE];
	bool found_t38 = false;
	data->mem_size = 0;

	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->object_table + i;

		reg = MXT_OBJECT_START + MXT_OBJECT_SIZE * i;
		error = mxt_read_object_table(data->client, reg, buf);
		if (error)
			return error;

		object->type = buf[0];
		object->start_address = (buf[2] << 8) | buf[1];
		object->size = buf[3];
		object->instances = buf[4];
		object->num_report_ids = buf[5];

		if (object->num_report_ids) {
			reportid += object->num_report_ids *
					(object->instances + 1);
			object->max_reportid = reportid;
		}

		/* Calculate index for config major version in config array.
		 * Major version is the first byte in object T38.
		 */
		if (object->type == MXT_SPT_USERDATA_T38) {
			data->t38_start_addr = object->start_address;
			found_t38 = true;
		}
		if (!found_t38 && mxt_object_writable(object->type))
			data->cfg_version_idx += object->size + 1;

		end_address = object->start_address
			+ (object->size + 1) * (object->instances + 1) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	return 0;
}

static int compare_crc(const u8 *v1, const u8 *v2)
{
	int i;

	if (!v1 || !v2)
		return -EINVAL;

	for (i = 0; i < MXT_CFG_VERSION_LEN; i++) {
		pr_info("compare: 0x%x, 0x%x\n", v1[i], v2[i]);
		if (v1[i] != v2[i])
		{
			pr_info("CRC not equal!\n");
			return MXT_CFG_VERSION_NOT_EQUAL;
		}
	}

	return MXT_CFG_VERSION_EQUAL;	/* v1 and v2 are equal */
}

#define CRC_OFFSET_IN_CONFIG	3
#define DEFAULT_FLAG_IN_CONFIG	7
static void mxt_check_config_crc(struct mxt_data *data,
			const struct mxt_config_info * cfg_info,
			bool *found_cfg_major_match)
{
	int result = -EINVAL;
	const u8 *config = cfg_info->config + data->cfg_version_idx;
	const u8 *config_crc = config + CRC_OFFSET_IN_CONFIG;

	result = compare_crc(data->in_chip_crc, config_crc);

	if (result >= MXT_CFG_VERSION_EQUAL) {
		*found_cfg_major_match = true;

		data->config_info = cfg_info;
		data->fw_name = cfg_info->fw_name;

		if (result != MXT_CFG_VERSION_EQUAL)
		{
			data->update_cfg = true;
			dev_info(&data->client->dev, "CRC not equal, update config.\n");
		} else
			dev_info(&data->client->dev, "CRC equal, keep config as the previous one.\n");
	}
}

static int mxt_get_lcd_id(struct mxt_data * data)
{
	int error;
	int ret;
	const struct mxt_platform_data *pdata = data->pdata;

	error = gpio_request(pdata->lcd_gpio, "mxt_lcd_gpio");
	if (error) {
		pr_err("%s: unable to request gpio [%d]\n", __func__,
					pdata->lcd_gpio);
		return -1;
	}
	error = gpio_direction_input(pdata->lcd_gpio);
	if (error) {
		pr_err("%s: unable to set_direction for gpio [%d]\n",
				__func__, pdata->lcd_gpio);
		return -1;
	}
	ret = gpio_get_value_cansleep(pdata->lcd_gpio);
	gpio_free(pdata->lcd_gpio);

	return ret;
}

#define ERR_NOT_FOUND_MATCHED_CONFIG	-1
static int mxt_get_default_config_index(struct mxt_data *data, int lcd_id)
{
	const struct mxt_platform_data *pdata = data->pdata;
	const struct mxt_config_info *cfg_info;
	const struct mxt_info *info = &data->info;
	const u8 *config;
	int flag = lcd_id + 1;
	int i;

	for (i = 0; i < pdata->config_array_size; i++) {
		cfg_info = &pdata->config_array[i];
		if (!cfg_info->config || !cfg_info->config_length)
			continue;
		config = cfg_info->config + data->cfg_version_idx;
		if (info->family_id == cfg_info->family_id &&
			info->variant_id == cfg_info->variant_id &&
			info->version == cfg_info->version &&
			info->build == cfg_info->build) {
			if (config[DEFAULT_FLAG_IN_CONFIG] == flag) {
				dev_info(&data->client->dev, "index = %d\n", i);
				return i;
			} else if(config[DEFAULT_FLAG_IN_CONFIG] == 0x00) {
				dev_info(&data->client->dev, "use default config\n");
				return i;
			}
		}
	}

	dev_info(&data->client->dev, "index = %d\n", i);
	return ERR_NOT_FOUND_MATCHED_CONFIG;
}

/* If the controller's config version has a non-zero major number, call this
 * function with match_major = true to look for the latest config present in
 * the pdata based on matching family id, variant id, f/w version, build, and
 * config major number. If the controller is programmed with wrong config data
 * previously, call this function with match_major = false to look for latest
 * config based on based on matching family id, variant id, f/w version and
 * build only.
 */
static int mxt_search_config_array(struct mxt_data *data)
{

	const struct mxt_platform_data *pdata = data->pdata;
	const struct mxt_config_info *cfg_info;
	bool found_cfg_major_match = false;
	int lcd_id;
	int index;

	lcd_id = mxt_get_lcd_id(data);
	if (lcd_id != -1)
		data->lcd_id = lcd_id;
	dev_info(&data->client->dev, "lcd id = %d\n", data->lcd_id);
	index = mxt_get_default_config_index(data, data->lcd_id);
	if (index == ERR_NOT_FOUND_MATCHED_CONFIG) {
		dev_err(&data->client->dev, "Haven't found matched config!\n");
		data->update_cfg = false;
		goto failed_area;
	}
	cfg_info = &pdata->config_array[index];
	mxt_check_config_crc(data, cfg_info, &found_cfg_major_match);
	if (found_cfg_major_match) {
		data->config_info = &pdata->config_array[index];
		data->fw_name =  pdata->config_array[index].fw_name;
		return 0;
	}

failed_area:
	data->config_info = NULL;
	data->fw_name = NULL;

	return -EINVAL;
}

static int mxt_get_config(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	struct device *dev = &data->client->dev;
	struct mxt_object *object;
	int error;

	if (!pdata->config_array || !pdata->config_array_size) {
		dev_dbg(dev, "No cfg data provided by platform data\n");
		return 0;
	}

	/* Get current config version */
	object = mxt_get_object(data, MXT_SPT_USERDATA_T38);
	if (!object) {
		dev_err(dev, "Unable to obtain USERDATA object\n");
		return -EINVAL;
	}

	error = __mxt_read_reg(data->client, object->start_address,
				sizeof(data->cfg_version), data->cfg_version);
	if (error) {
		dev_err(dev, "Unable to read config version\n");
		return error;
	}
	dev_info(dev, "Current config version on the controller is %d.%d.%d\n",
			data->cfg_version[0], data->cfg_version[1],
			data->cfg_version[2]);

	error = mxt_search_config_array(data);
	if (error) {
		dev_err(dev, "Unable to find matching config in pdata\n");
		return error;
	}

	return 0;
}

static void mxt_reset_delay(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	const struct mxt_config_info *mcfg_info = pdata->config_array;
	struct mxt_info *info = &data->info;
	u8 family_id;

	if (info->family_id == 0) {
		family_id = mcfg_info[0].family_id;
	} else {
		family_id = info->family_id;
	}

	switch (family_id) {
	case MXT224_ID:
		msleep(MXT224_RESET_TIME);
		break;
	case MXT224E_ID:
		msleep(MXT224E_RESET_TIME);
		break;
	case MXT336S_ID:
		msleep(MXT336S_RESET_TIME);
		break;
	case MXT384E_ID:
		msleep(MXT384E_RESET_TIME);
		break;
	case MXT1386_ID:
		msleep(MXT1386_RESET_TIME);
		break;
	default:
		msleep(MXT_RESET_TIME);
	}
}

static int mxt_backup_nv(struct mxt_data *data)
{
	int error;
	u8 command_register;
	int timeout_counter = 0;

	/* Backup to memory */
	mxt_write_object(data, MXT_GEN_COMMAND_T6,
			MXT_COMMAND_BACKUPNV,
			MXT_BACKUP_VALUE);
	msleep(MXT_BACKUP_TIME);

	do {
		error = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					MXT_COMMAND_BACKUPNV,
					&command_register);
		if (error)
			return error;

		usleep_range(1000, 2000);

	} while ((command_register != 0) && (++timeout_counter <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "No response after backup!\n");
		return -EIO;
	}

	/* Soft reset */
	mxt_write_object(data, MXT_GEN_COMMAND_T6, MXT_COMMAND_RESET, 1);

	mxt_reset_delay(data);

	return 0;
}

static int mxt_save_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_object *t7_object;
	struct mxt_object *t8_object;
	struct mxt_object *t9_object;
	struct mxt_object *t15_object;
	struct mxt_object *t25_object;
	struct mxt_object *t42_object;
	struct mxt_object *t37_object;
	struct mxt_object *t6_object;

	t6_object = mxt_get_object(data, MXT_GEN_COMMAND_T6);
	if (!t6_object) {
		dev_err(&client->dev, "Failed to get T6 object\n");
		return -EINVAL;
	}
	data->t6_reportid = t6_object->max_reportid;

	/* Store T7 and T8 locally, used in suspend/resume operations */
	t7_object = mxt_get_object(data, MXT_GEN_POWER_T7);
	if (!t7_object) {
		dev_err(&client->dev, "Failed to get T7 object\n");
		return -EINVAL;
	}

	data->t7_start_addr = t7_object->start_address;

	t8_object = mxt_get_object(data, MXT_GEN_ACQUIRE_T8);
	if (!t8_object) {
		dev_err(&client->dev, "Failed to get T8 object\n");
		return -EINVAL;
	}

	data->t8_start_addr = t8_object->start_address;

	/* Store T9, T15's min and max report ids */
	t9_object = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (!t9_object) {
		dev_err(&client->dev, "Failed to get T9 object\n");
		return -EINVAL;
	}
	data->t9_max_reportid = t9_object->max_reportid;
	data->t9_min_reportid = t9_object->max_reportid -
		(t9_object->instances + 1) * t9_object->num_report_ids + 1;

	if (data->pdata->key_codes) {
		t15_object = mxt_get_object(data, MXT_TOUCH_KEYARRAY_T15);
		if (!t15_object)
			dev_dbg(&client->dev, "T15 object is not available\n");
		else {
			data->t15_max_reportid = t15_object->max_reportid;
			data->t15_min_reportid = t15_object->max_reportid -
				(t15_object->instances + 1) * t15_object->num_report_ids + 1;
		}
	}

	/* Store T25 min and max report ids */
	t25_object = mxt_get_object(data, MXT_SPT_SELFTEST_T25);
	if (!t25_object)
		dev_dbg(&client->dev, "T25 object is not available\n");
	else {
		data->t25_max_reportid = t25_object->max_reportid;
		data->t25_min_reportid = t25_object->max_reportid -
					t25_object->num_report_ids + 1;
	}

	/* Store T42 min and max report ids */
	t42_object = mxt_get_object(data, MXT_PROCI_TOUCHSUPPRESSION_T42);
	if (!t42_object)
		dev_dbg(&client->dev, "T42 object is not available\n");
	else {
		data->t42_max_reportid = t42_object->max_reportid;
		data->t42_min_reportid = t42_object->max_reportid -
					t42_object->num_report_ids + 1;
	}

	/* Store T37 min and max report ids */
	t37_object = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!t37_object)
		dev_dbg(&client->dev, "T37 object is not available\n");
	else {
		data->t37_start_addr = t37_object->start_address;
		data->t37_object_size = t37_object->size - 1;
	}

	return 0;
}

static void mxt_reset_toggle(struct mxt_data *data);
static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

#define BOOTLOADER_MODE	0x0
#define NO_CONFIG_MODE		0x1
static int mxt_update_fw_for_abnormal(struct mxt_data *data, int mode)
{
	int error;
	unsigned int state;
	struct i2c_client *client = data->client;

	if (mode == NO_CONFIG_MODE) {
		mxt_reset_toggle(data);
		msleep(100);
		state = MXT_WAITING_BOOTLOAD_CMD;
	} else
		state = MXT_APP_CRC_FAIL;

	error = mxt_switch_to_bootloader_address(data);
	if (error)
		return error;

	error = mxt_check_bootloader(client, state);
	if (error)
		return error;

	dev_err(&client->dev, "Application CRC failure\n");
	data->state = BOOTLOADER;

	error = mxt_update_fw_store(&client->dev, NULL, NULL, 0);
	if (error)
		dev_err(&client->dev, "Unable to update firmware!\n");

	return error;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;
	u8 val;
	const u8 *cfg_ver;

	error = mxt_get_info(data);
	if (error) {
		/* Try bootloader mode */
		error = mxt_update_fw_for_abnormal(data, BOOTLOADER_MODE);
		if (error)
			return error;
		return 0;
	}

	dev_info(&client->dev,
			"Family ID: %02X Variant ID: %02X Version: %d.%d "
			"Build: 0x%02X Object Num: %d\n",
			info->family_id, info->variant_id,
			info->version >> 4, info->version & 0xf,
			info->build, info->object_num);

	data->state = APPMODE;

	data->object_table = kcalloc(info->object_num,
				     sizeof(struct mxt_object),
				     GFP_KERNEL);
	if (!data->object_table) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Get object table information */
	error = mxt_get_object_table(data);
	if (error)
		goto free_object_table;

	error = mxt_save_objects(data);
	if (error)
		goto free_object_table;

	mxt_get_crc(data);

	/* Get config data from platform data */
	error = mxt_get_config(data);
	if (error)
		dev_dbg(&client->dev, "Config info not found.\n");

	/* Check register init values */
	if (data->config_info && data->config_info->config) {
		if (data->update_cfg) {
			error = mxt_check_reg_init(data);
			if (error) {
				dev_err(&client->dev,
					"Failed to check reg init value\n");
				goto free_object_table;
			}

			error = mxt_backup_nv(data);
			if (error) {
				dev_err(&client->dev, "Failed to back up NV\n");
				goto free_object_table;
			}

			cfg_ver = data->config_info->config +
							data->cfg_version_idx;
			dev_info(&client->dev,
				"Config updated from %d.%d.%d to %d.%d.%d\n",
				data->cfg_version[0], data->cfg_version[1],
				data->cfg_version[2],
				cfg_ver[0], cfg_ver[1], cfg_ver[2]);

			memcpy(data->cfg_version, cfg_ver, MXT_CFG_VERSION_LEN);
		}
	} else {
		dev_info(&client->dev,
			"No cfg data defined, skipping check reg init\n");
		error = mxt_update_fw_for_abnormal(data, NO_CONFIG_MODE);
		if (error)
			return error;
		return 0;
	}

	error = __mxt_read_reg(client, data->t7_start_addr,
				T7_DATA_SIZE, data->t7_data);
	if (error < 0) {
		dev_err(&client->dev,
			"Failed to save current power state\n");
		goto free_object_table;
	}

	error = __mxt_read_reg(client, data->t8_start_addr,
				T8_DATA_SIZE, data->t8_data);
	if (error < 0) {
		dev_err(&client->dev,
			"Failed to save current anti-touch setting\n");
		goto free_object_table;
	}

	/* Update matrix size at info struct */
	error = mxt_read_reg(client, MXT_MATRIX_X_SIZE, &val);
	if (error)
		goto free_object_table;
	info->matrix_xsize = val;

	error = mxt_read_reg(client, MXT_MATRIX_Y_SIZE, &val);
	if (error)
		goto free_object_table;
	info->matrix_ysize = val;

	dev_info(&client->dev,
			"Matrix X Size: %d Matrix Y Size: %d\n",
			info->matrix_xsize, info->matrix_ysize);

	return 0;

free_object_table:
	kfree(data->object_table);
	return error;
}

static ssize_t mxt_dbgdump_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%lu\n", data->dbgdump);
}

static ssize_t mxt_dbgdump_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return strict_strtoul(buf, 0, &data->dbgdump) < 0 ? 0 : count;
}

static ssize_t mxt_selftest_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%02x, %02x, %02x, %02x, %02x, %02x\n",
			data->test_result[0], data->test_result[1],
			data->test_result[2], data->test_result[3],
			data->test_result[4], data->test_result[5]);
}

static ssize_t mxt_selftest_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 shieldless_ctrl, dualx_ctrl, selftest_cmd;
	int error;

	/* disable shieldless per datasheet */
	error = mxt_read_object(data,
			MXT_PROCI_SHIELDLESS_T56,
			0x00, &shieldless_ctrl);
	if (error)
		return error;

	error = mxt_write_object(data,
			MXT_PROCI_SHIELDLESS_T56,
			0x00, 0x00);
	if (error)
		return error;

	error = mxt_read_object(data,
			MXT_PROCG_NOISESUPPRESSION_T62,
			0x03, &dualx_ctrl);
	if (error)
		return error;

	error = mxt_write_object(data,
			MXT_PROCG_NOISESUPPRESSION_T62,
			0x03, (dualx_ctrl & 0xFE));
	if (error)
		return error;

	msleep(1000);

	/* run all selftest */
	error = mxt_write_object(data,
			MXT_SPT_SELFTEST_T25,
			0x01, 0xfe);

	if (!error) {
		while (true) {
			msleep(10);
			error = mxt_read_object(data,
					MXT_SPT_SELFTEST_T25,
					0x01, &selftest_cmd);
			if (error || selftest_cmd == 0)
				break;
		}
	}

	/* restore shieldless setting */
	mxt_write_object(data,
			MXT_PROCI_SHIELDLESS_T56,
			0x00, shieldless_ctrl);
	/* restore dual-x setting */
	mxt_write_object(data,
			MXT_PROCG_NOISESUPPRESSION_T62,
			0x03, dualx_ctrl);

	return error ? : count;
}

static int strtobyte(const char *data, u8 *value)
{
	char str[3];

	str[0] = data[0];
	str[1] = data[1];
	str[2] = '\0';

	return kstrtou8(str, 16, value);
}

static void mxt_reset_toggle(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	int i;

	for (i = 0; i < 10; i++) {
		gpio_set_value_cansleep(pdata->reset_gpio, 1);
		msleep(1);
		gpio_set_value_cansleep(pdata->reset_gpio, 0);
		msleep(60);
	}

	gpio_set_value_cansleep(pdata->reset_gpio, 1);
}

static int mxt_load_fw(struct device *dev, const char *fn)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int retry = 0;
	unsigned int pos = 0;
	int ret, i, max_frame_size;
	u8 *frame;

	switch (data->info.family_id) {
	case MXT224_ID:
	case MXT224E_ID:
	case MXT336S_ID:
		max_frame_size = MXT_SINGLE_FW_MAX_FRAME_SIZE;
		break;
	case MXT384E_ID:
	case MXT1386_ID:
		max_frame_size = MXT_CHIPSET_FW_MAX_FRAME_SIZE;
		break;
	default:
		return -EINVAL;
	}

	frame = kmalloc(max_frame_size, GFP_KERNEL);
	if (!frame) {
		dev_err(dev, "Unable to allocate memory for frame data\n");
		return -ENOMEM;
	}

	ret = request_firmware(&fw, fn, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		goto free_frame;
	}

	if (data->state != BOOTLOADER) {
		/* Change to the bootloader mode */
		mxt_reset_toggle(data);
		msleep(100);
		ret = mxt_switch_to_bootloader_address(data);
		if (ret)
			goto release_firmware;
	}

	ret = mxt_check_bootloader(client, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(client,
			MXT_WAITING_FRAME_DATA);

		if (ret)
			goto return_to_app_mode;
	} else {
		dev_info(dev, "Unlocking bootloader\n");
		/* Unlock bootloader */
		mxt_unlock_bootloader(client);
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(client,
						MXT_WAITING_FRAME_DATA);
		if (ret)
			goto release_firmware;

		/* Get frame length MSB */
		ret = strtobyte(fw->data + pos, frame);
		if (ret)
			goto release_firmware;

		/* Get frame length LSB */
		ret = strtobyte(fw->data + pos + 2, frame + 1);
		if (ret)
			goto release_firmware;

		frame_size = ((*frame << 8) | *(frame + 1));

		/* We should add 2 at frame size as the the firmware data is not
		 * included the CRC bytes.
		 */
		frame_size += 2;

		if (frame_size > max_frame_size) {
			dev_err(dev, "Invalid frame size - %d\n", frame_size);
			ret = -EINVAL;
			goto release_firmware;
		}

		/* Convert frame data and CRC from hex to binary */
		for (i = 2; i < frame_size; i++) {
			ret = strtobyte(fw->data + pos + i * 2, frame + i);
			if (ret)
				goto release_firmware;
		}

		/* Write one frame to device */
		mxt_fw_write(client, frame, frame_size);

		ret = mxt_check_bootloader(client,
						MXT_FRAME_CRC_PASS);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20)
				goto release_firmware;
		} else {
			retry = 0;
			pos += frame_size * 2;
			dev_info(dev, "Updated %d/%zd bytes\n", pos, fw->size);
		}
	}

return_to_app_mode:
	mxt_switch_to_appmode_address(data);
release_firmware:
	release_firmware(fw);
free_frame:
	kfree(frame);

	return ret;
}

static const char *
mxt_search_fw_name(struct mxt_data *data, u8 bootldr_id)
{
	const struct mxt_platform_data *pdata = data->pdata;
	const struct mxt_config_info *cfg_info;
	const char *fw_name = NULL;
	int i;

	for (i = 0; i < pdata->config_array_size; i++) {
		cfg_info = &pdata->config_array[i];
		if (bootldr_id == cfg_info->bootldr_id && cfg_info->fw_name) {
			data->config_info = cfg_info;
			data->info.family_id = cfg_info->family_id;
			fw_name = cfg_info->fw_name;
		}
	}

	return fw_name;
}

static ssize_t mxt_update_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count = sprintf(buf,
			"family_id=0x%x, variant_id=0x%x, version=0x%x, build=0x%x\n",
			data->info.family_id, data->info.variant_id,
			data->info.version, data->info.build);
	return count;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	const char *fw_name;
	u8 bootldr_id;
	struct input_dev *input_dev = data->input_dev;

	if (count > 0) {
		fw_name = buf;
		dev_info(dev, "Identify firmware name :%s \n", fw_name);
	}
	/* If fw_name is set, then the existing firmware has an upgrade */
	else if (!data->fw_name) {
	/*
	* If the device boots up in the bootloader mode, check if
	* there is a firmware to upgrade.
	*/
		if (data->state == BOOTLOADER) {
			bootldr_id = mxt_get_bootloader_id(data->client);
			if (bootldr_id <= 0) {
				dev_err(dev,
					"Unable to retrieve bootloader id\n");
				return -EINVAL;
			}
			fw_name = mxt_search_fw_name(data, bootldr_id);
			if (fw_name == NULL) {
				dev_err(dev,
				"Unable to find fw from bootloader id\n");
				return -EINVAL;
			}
		} else {
			/* In APPMODE, if the f/w name does not exist, quit */
			dev_err(dev,
			"Firmware name not specified in platform data\n");
			return -EINVAL;
		}
	} else {
		fw_name = data->fw_name;
	}

	dev_info(dev, "Upgrading the firmware file to %s\n", fw_name);

	mutex_lock(&input_dev->mutex);
	disable_irq(data->irq);
	error = mxt_load_fw(dev, fw_name);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		/* Wait for reset */
		msleep(MXT_FWRESET_TIME);

		data->state = INIT;
		kfree(data->object_table);
		data->object_table = NULL;
		data->cfg_version_idx = 0;
		data->update_cfg = false;
		data->is_crc_got = false;
		mxt_initialize(data);
	}

	if (data->state == APPMODE) {
		enable_irq(data->irq);

		error = mxt_make_highchg(data);
		if (error) {
			count = error;
		}
	}

	data->is_key_verify = false;
	mxt_handle_extra_touchdata(data, MXT_REFERENCE_DATA);
	mutex_unlock(&input_dev->mutex);
	return count;
}

static ssize_t mxt_pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;
	char c;

	c = data->driver_paused ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_pause_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->driver_paused = (i == 1);
		dev_dbg(dev, "%s\n", i ? "paused" : "unpaused");
		return count;
	} else {
		dev_dbg(dev, "pause_driver write error\n");
		return -EINVAL;
	}
}

static int mxt_enable_disable_t72_report(struct mxt_data *data, bool enable)
{
	int error;
	u8 val;

	val = enable ? 0xFF : 0x1;

	error = mxt_write_object(data,
			MXT_PROCG_NOISESUPPRESSION_T72,
			0x00, val);

	return error;
}

static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	char c;

	c = data->debug_enabled ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);
		if (mxt_get_object(data, MXT_PROCG_NOISESUPPRESSION_T72) != NULL) {
			if (data->debug_enabled)
				mxt_enable_disable_t72_report(data, true);
			else
				mxt_enable_disable_t72_report(data, false);
		}

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_update_fw_flag_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;
	int i;

	if (sscanf(buf, "%u", &i) == 1)  {
		dev_dbg(dev, "write fw update flag %d to t38\n", i);
		ret = mxt_write_object(data, MXT_SPT_USERDATA_T38,
					MXT_FW_UPDATE_FLAG, (u8)i);
		if (ret < 0)
			return ret;
		ret = mxt_backup_nv(data);
		if (ret)
			return ret;
	}

	return count;
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}

static DEVICE_ATTR(dbgdump, 0664, mxt_dbgdump_show, mxt_dbgdump_store);
static DEVICE_ATTR(selftest, 0664, mxt_selftest_show, mxt_selftest_store);
static DEVICE_ATTR(update_fw, 0664, mxt_update_fw_show, mxt_update_fw_store);
static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
		   mxt_debug_enable_store);
static DEVICE_ATTR(pause_driver, S_IWUSR | S_IRUSR, mxt_pause_show,
		   mxt_pause_store);
static DEVICE_ATTR(update_fw_flag, 0200, NULL, mxt_update_fw_flag_store);

static struct attribute *mxt_attrs[] = {
	&dev_attr_dbgdump.attr,
	&dev_attr_selftest.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_pause_driver.attr,
	&dev_attr_update_fw_flag.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static int mxt_start(struct mxt_data *data)
{
	int error;

	/* restore the old power state values and reenable touch */
	error = __mxt_write_reg(data->client, data->t7_start_addr,
				T7_DATA_SIZE, data->t7_data);
	if (error < 0) {
		dev_err(&data->client->dev,
			"failed to restore old power state\n");
		return error;
	}

	return 0;
}

static int mxt_stop(struct mxt_data *data)
{
	int error;
	u8 t7_data[T7_DATA_SIZE] = {0};

	error = __mxt_write_reg(data->client, data->t7_start_addr,
				T7_DATA_SIZE, t7_data);
	if (error < 0) {
		dev_err(&data->client->dev,
			"failed to configure deep sleep mode\n");
		return error;
	}

	return 0;
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	int error;

	if (data->state == APPMODE) {
		error = mxt_start(data);
		if (error < 0) {
			dev_err(&data->client->dev, "mxt_start failed in input_open\n");
			return error;
		}
	}

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	int error;

	if (data->state == APPMODE) {
		error = mxt_stop(data);
		if (error < 0)
			dev_err(&data->client->dev, "mxt_stop failed in input_close\n");
	}
}

#define CALIB_TIMEOUT 1000
static void mxt_do_force_calibration(struct mxt_data *data)
{
	int error;
	u8 val;
	int i = 0;

	error = mxt_write_object(data,
		MXT_GEN_COMMAND_T6,
		MXT_COMMAND_CALIBRATE, 0x01);
	if (error < 0) {
		goto error_seg;
	}

	while(i < CALIB_TIMEOUT) {
		error = mxt_read_object(data,
			MXT_GEN_COMMAND_T6,
			MXT_COMMAND_CALIBRATE, &val);
		if (error < 0) {
			goto error_seg;
		}

		if ((val & 0x1) == 0)
			break;

		i ++;
		mdelay(1);
	}

	return;
error_seg:
	dev_err(&data->client->dev, "calibration failed in %s\n", __func__);
}

static void mxt_force_calibrate_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct mxt_data *data = container_of(delayed_work, struct mxt_data, force_calibrate_delayed_work);

	mxt_do_force_calibration(data);
}

static int mxt_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct mxt_data *data = input_get_drvdata(dev);

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value) {
			if (data->dbgdump >= 1) {
				dev_info(&data->client->dev,
					"calibrate since device turn on with shelter\n");
			}
			schedule_delayed_work(&data->force_calibrate_delayed_work, MXT_FORCE_CALIBRATE_DELAY);
			schedule_delayed_work(&data->disable_antipalm_delayed_work, MXT_STOP_ANTIPALM_TIMEOUT);
		} else {
			if (data->dbgdump >= 1) {
				dev_info(&data->client->dev,
					"Proximity sensor is shelterred.\n");
			}
			cancel_delayed_work(&data->disable_antipalm_delayed_work);
		}
	}

	return 0;
}

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (reg && regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int mxt_power_on(struct mxt_data *data, bool on)
{
	int rc;

	if (on == false)
		goto power_off;

	rc = reg_set_optimum_mode_check(data->vcc_ana, MXT_ACTIVE_LOAD_UA);
	if (rc < 0) {
		dev_err(&data->client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_ana);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_ana enable failed rc=%d\n", rc);
		goto error_reg_en_vcc_ana;
	}

	if (data->pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(data->vcc_dig,
					MXT_ACTIVE_LOAD_DIG_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_dig set_opt failed rc=%d\n",
				rc);
			goto error_reg_opt_vcc_dig;
		}

		rc = regulator_enable(data->vcc_dig);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vcc_dig enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_dig;
		}
	}

	if (data->pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(data->vcc_i2c, MXT_I2C_LOAD_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(data->vcc_i2c);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}

	msleep(130);

	return 0;

error_reg_en_vcc_i2c:
	if (data->pdata->i2c_pull_up)
		reg_set_optimum_mode_check(data->vcc_i2c, 0);
error_reg_opt_i2c:
	if (data->pdata->digital_pwr_regulator)
		regulator_disable(data->vcc_dig);
error_reg_en_vcc_dig:
	if (data->pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(data->vcc_dig, 0);
error_reg_opt_vcc_dig:
	regulator_disable(data->vcc_ana);
error_reg_en_vcc_ana:
	reg_set_optimum_mode_check(data->vcc_ana, 0);
	return rc;

power_off:
	reg_set_optimum_mode_check(data->vcc_ana, 0);
	regulator_disable(data->vcc_ana);
	if (data->pdata->digital_pwr_regulator) {
		reg_set_optimum_mode_check(data->vcc_dig, 0);
		regulator_disable(data->vcc_dig);
	}
	if (data->pdata->i2c_pull_up) {
		reg_set_optimum_mode_check(data->vcc_i2c, 0);
		regulator_disable(data->vcc_i2c);
	}
	msleep(50);
	return 0;
}

static int mxt_regulator_configure(struct mxt_data *data, bool on)
{
	int rc;

	if (on == false)
		goto hw_shutdown;

	data->vcc_ana = regulator_get(&data->client->dev, "vdd_ana");
	if (IS_ERR(data->vcc_ana)) {
		rc = PTR_ERR(data->vcc_ana);
		dev_err(&data->client->dev,
			"Regulator get failed vcc_ana rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vcc_ana) > 0) {
		rc = regulator_set_voltage(data->vcc_ana, MXT_VTG_MIN_UV,
							MXT_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
				"regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_vcc_ana;
		}
	}
	if (data->pdata->digital_pwr_regulator) {
		data->vcc_dig = regulator_get(&data->client->dev, "vdd_dig");
		if (IS_ERR(data->vcc_dig)) {
			rc = PTR_ERR(data->vcc_dig);
			dev_err(&data->client->dev,
				"Regulator get dig failed rc=%d\n", rc);
			goto error_get_vtg_vcc_dig;
		}

		if (regulator_count_voltages(data->vcc_dig) > 0) {
			rc = regulator_set_voltage(data->vcc_dig,
				MXT_VTG_DIG_MIN_UV, MXT_VTG_DIG_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_vcc_dig;
			}
		}
	}
	if (data->pdata->i2c_pull_up) {
		data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
		if (IS_ERR(data->vcc_i2c)) {
			rc = PTR_ERR(data->vcc_i2c);
			dev_err(&data->client->dev,
				"Regulator get failed rc=%d\n",	rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(data->vcc_i2c) > 0) {
			rc = regulator_set_voltage(data->vcc_i2c,
				MXT_I2C_VTG_MIN_UV, MXT_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}
	}

	return 0;

error_set_vtg_i2c:
	regulator_put(data->vcc_i2c);
error_get_vtg_i2c:
	if (data->pdata->digital_pwr_regulator)
		if (regulator_count_voltages(data->vcc_dig) > 0)
			regulator_set_voltage(data->vcc_dig, 0,
				MXT_VTG_DIG_MAX_UV);
error_set_vtg_vcc_dig:
	if (data->pdata->digital_pwr_regulator)
		regulator_put(data->vcc_dig);
error_get_vtg_vcc_dig:
	if (regulator_count_voltages(data->vcc_ana) > 0)
		regulator_set_voltage(data->vcc_ana, 0, MXT_VTG_MAX_UV);
error_set_vtg_vcc_ana:
	regulator_put(data->vcc_ana);
	return rc;

hw_shutdown:
	if (regulator_count_voltages(data->vcc_ana) > 0)
		regulator_set_voltage(data->vcc_ana, 0, MXT_VTG_MAX_UV);
	regulator_put(data->vcc_ana);
	if (data->pdata->digital_pwr_regulator) {
		if (regulator_count_voltages(data->vcc_dig) > 0)
			regulator_set_voltage(data->vcc_dig, 0,
						MXT_VTG_DIG_MAX_UV);
		regulator_put(data->vcc_dig);
	}
	if (data->pdata->i2c_pull_up) {
		if (regulator_count_voltages(data->vcc_i2c) > 0)
			regulator_set_voltage(data->vcc_i2c, 0,
						MXT_I2C_VTG_MAX_UV);
		regulator_put(data->vcc_i2c);
	}
	return 0;
}

#ifdef CONFIG_PM
static int mxt_regulator_lpm(struct mxt_data *data, bool on)
{

	int rc;

	if (on == false)
		goto regulator_hpm;

	rc = reg_set_optimum_mode_check(data->vcc_ana, MXT_LPM_LOAD_UA);
	if (rc < 0) {
		dev_err(&data->client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		goto fail_regulator_lpm;
	}

	if (data->pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(data->vcc_dig,
						MXT_LPM_LOAD_DIG_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_dig set_opt failed rc=%d\n", rc);
			goto fail_regulator_lpm;
		}
	}

	if (data->pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(data->vcc_i2c,
						MXT_I2C_LPM_LOAD_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto fail_regulator_lpm;
		}
	}

	return 0;

regulator_hpm:

	rc = reg_set_optimum_mode_check(data->vcc_ana, MXT_ACTIVE_LOAD_UA);
	if (rc < 0) {
		dev_err(&data->client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		goto fail_regulator_hpm;
	}

	if (data->pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(data->vcc_dig,
						 MXT_ACTIVE_LOAD_DIG_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_dig set_opt failed rc=%d\n", rc);
			goto fail_regulator_hpm;
		}
	}

	if (data->pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(data->vcc_i2c, MXT_I2C_LOAD_UA);
		if (rc < 0) {
			dev_err(&data->client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto fail_regulator_hpm;
		}
	}

	return 0;

fail_regulator_lpm:
	reg_set_optimum_mode_check(data->vcc_ana, MXT_ACTIVE_LOAD_UA);
	if (data->pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(data->vcc_dig,
					MXT_ACTIVE_LOAD_DIG_UA);
	if (data->pdata->i2c_pull_up)
		reg_set_optimum_mode_check(data->vcc_i2c, MXT_I2C_LOAD_UA);

	return rc;

fail_regulator_hpm:
	reg_set_optimum_mode_check(data->vcc_ana, MXT_LPM_LOAD_UA);
	if (data->pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(data->vcc_dig, MXT_LPM_LOAD_DIG_UA);
	if (data->pdata->i2c_pull_up)
		reg_set_optimum_mode_check(data->vcc_i2c, MXT_I2C_LPM_LOAD_UA);

	return rc;
}

static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	int error;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users) {
		error = mxt_stop(data);
		if (error < 0) {
			dev_err(dev, "mxt_stop failed in suspend\n");
			mutex_unlock(&input_dev->mutex);
			return error;
		}

	}

	mutex_unlock(&input_dev->mutex);

	cancel_delayed_work_sync(&data->force_calibrate_delayed_work);
	cancel_delayed_work_sync(&data->disable_antipalm_delayed_work);

	mxt_anti_palm_control(data, NORMAL_STATE);
	mxt_clear_touch_event(data);

	/* put regulators in low power mode */
	error = mxt_regulator_lpm(data, true);
	if (error < 0) {
		dev_err(dev, "failed to enter low power mode\n");
		return error;
	}

	data->disable_antipalm_done = false;
	data->is_land_signed = false;
	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	int error;

	/* put regulators in high power mode */
	error = mxt_regulator_lpm(data, false);
	if (error < 0) {
		dev_err(dev, "failed to enter high power mode\n");
		return error;
	}

	mutex_lock(&input_dev->mutex);

	if (input_dev->users) {
		error = mxt_start(data);
		if (error < 0) {
			dev_err(dev, "mxt_start failed in resume\n");
			mutex_unlock(&input_dev->mutex);
			return error;
		}
	}

	mxt_handle_extra_touchdata(data, MXT_DELTA_DATA);
	mxt_handle_extra_touchdata(data, MXT_REFERENCE_DATA);

	/* force calibration in resume */
	mxt_do_force_calibration(data);
	schedule_delayed_work(&data->disable_antipalm_delayed_work, MXT_STOP_ANTIPALM_TIMEOUT);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mxt_early_suspend(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data, early_suspend);

	mxt_suspend(&data->client->dev);
}

static void mxt_late_resume(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data, early_suspend);

	mxt_resume(&data->client->dev);
}
#endif

static const struct dev_pm_ops mxt_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= mxt_suspend,
	.resume		= mxt_resume,
#endif
};
#endif

static int mxt_debugfs_object_show(struct seq_file *m, void *v)
{
	struct mxt_data *data = m->private;
	struct mxt_object *object;
	struct device *dev = &data->client->dev;
	int i, j, k;
	int error;
	int obj_size;
	u8 val;

	seq_printf(m,
		  "Family ID: %02X Variant ID: %02X Version: %d.%d Build: 0x%02X"
		  "\nObject Num: %dMatrix X Size: %d Matrix Y Size: %d\n",
		   data->info.family_id, data->info.variant_id,
		   data->info.version >> 4, data->info.version & 0xf,
		   data->info.build, data->info.object_num,
		   data->info.matrix_xsize, data->info.matrix_ysize);

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		obj_size = object->size + 1;

		for (j = 0; j < object->instances + 1; j++) {
			seq_printf(m, "Type %d NumId %d MaxId %d\n",
				   object->type, object->num_report_ids,
				   object->max_reportid);

			for (k = 0; k < obj_size; k++) {
				error = mxt_read_object(data, object->type,
							j * obj_size + k, &val);
				if (error) {
					dev_err(dev,
						"Failed to read object %d "
						"instance %d at offset %d\n",
						object->type, j, k);
					return error;
				}

				seq_printf(m, "%02x ", val);
				if (k % 10 == 9 || k + 1 == obj_size)
					seq_printf(m, "\n");
			}
		}
	}

	return 0;
}

static ssize_t mxt_debugfs_object_store(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mxt_data *data = m->private;
	u8 type, offset, val;
	int error;

	if (sscanf(buf, "%hhu:%hhu=%hhx", &type, &offset, &val) == 3) {
		error = mxt_write_object(data, type, offset, val);
		if (error)
			count = error;
	} else
		count = -EINVAL;

	return count;
}

static int mxt_debugfs_object_open(struct inode *inode, struct file *file)
{
	return single_open(file, mxt_debugfs_object_show, inode->i_private);
}

static const struct file_operations mxt_object_fops = {
	.owner		= THIS_MODULE,
	.open		= mxt_debugfs_object_open,
	.read		= seq_read,
	.write		= mxt_debugfs_object_store,
	.release	= single_release,
};

static void __devinit mxt_debugfs_init(struct mxt_data *data)
{
	debug_base = debugfs_create_dir(MXT_DEBUGFS_DIR, NULL);
	if (IS_ERR_OR_NULL(debug_base))
		pr_err("atmel_mxt_ts: Failed to create debugfs dir\n");
	if (IS_ERR_OR_NULL(debugfs_create_file(MXT_DEBUGFS_FILE,
					       0444,
					       debug_base,
					       data,
					       &mxt_object_fops))) {
		pr_err("atmel_mxt_ts: Failed to create object file\n");
		debugfs_remove_recursive(debug_base);
	}
}

static int mxt_is_irq_assert(struct mxt_data *data)
{
	struct mxt_platform_data *pdata = data->client->dev.platform_data;

	if (pdata->irqflags & IRQF_TRIGGER_LOW)
		return gpio_get_value_cansleep(pdata->irq_gpio) == 0;
	else
		return gpio_get_value_cansleep(pdata->irq_gpio) == 1;
}

static int mxt_wait_irq_assert(struct mxt_data *data)
{
	int i;

	for  (i = 0; i < 10; i++) {
		if (mxt_is_irq_assert(data))
			return 0;
		msleep(10);
	}

	return -EBUSY;
}

static int __devinit mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	const struct mxt_platform_data *pdata = client->dev.platform_data;
	struct mxt_data *data;
	struct input_dev *input_dev;
	int error, i;

	if (!pdata)
		return -EINVAL;

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	data->state = INIT;
	data->dbgdump = 1;
	input_dev->name = "atmel_mxt_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;
	input_dev->event = mxt_input_event;

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = pdata;
	data->irq = client->irq;

	INIT_DELAYED_WORK(&data->force_calibrate_delayed_work, mxt_force_calibrate_delayed_work);
	INIT_DELAYED_WORK(&data->disable_antipalm_delayed_work, mxt_disable_antipalm_delayed_work);

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	/* For multi touch */
	input_mt_init_slots(input_dev, MXT_MAX_FINGER);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			pdata->disp_minx, pdata->disp_maxx, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			pdata->disp_miny, pdata->disp_maxy, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);

	/* set key array supported keys */
	if (pdata->key_codes) {
		for (i = 0; i < MXT_KEYARRAY_MAX_KEYS; i++) {
			if (pdata->key_codes[i])
				input_set_capability(input_dev, EV_KEY,
							pdata->key_codes[i]);
		}
	}

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	if (pdata->init_hw)
		error = pdata->init_hw(true);
	else
		error = mxt_regulator_configure(data, true);
	if (error) {
		dev_err(&client->dev, "Failed to intialize hardware\n");
		goto err_free_mem;
	}

	if (pdata->power_on)
		error = pdata->power_on(true);
	else
		error = mxt_power_on(data, true);
	if (error) {
		dev_err(&client->dev, "Failed to power on hardware\n");
		goto err_regulator_on;
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->irq_gpio,
							"mxt_irq_gpio");
		if (error) {
			pr_err("%s: unable to request gpio [%d]\n", __func__,
						pdata->irq_gpio);
			goto err_power_on;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error) {
			pr_err("%s: unable to set_direction for gpio [%d]\n",
					__func__, pdata->irq_gpio);
			goto err_irq_gpio_req;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->reset_gpio,
						"mxt_reset_gpio");
		if (error) {
			pr_err("%s: unable to request reset gpio %d\n",
				__func__, pdata->reset_gpio);
			goto err_irq_gpio_req;
		}

		error = gpio_direction_output(
					pdata->reset_gpio, 1);
		if (error) {
			pr_err("%s: unable to set direction for gpio %d\n",
				__func__, pdata->reset_gpio);
			goto err_reset_gpio_req;
		}
	}

	error = mxt_wait_irq_assert(data);
	if (error) {
		dev_err(&client->dev, "wait irq assert timeout!\n");
		goto err_reset_gpio_req;
	}

	error = mxt_initialize(data);
	if (error)
		goto err_reset_gpio_req;

	if (data->cfg_version[MXT_FW_UPDATE_FLAG] == 0x01) {
		error = mxt_update_fw_flag_store(&client->dev, NULL, "0", 2);
		if (error != 2)
			dev_err(&client->dev, "Failed to set T38 flag to 0!\n");
		else {
			error = mxt_update_fw_store(&client->dev, NULL, NULL, 0);
			if (error)
				dev_err(&client->dev, "Unable to update firmware!\n");
		}
	}

	error = request_threaded_irq(client->irq, NULL, mxt_interrupt,
			pdata->irqflags, client->dev.driver->name, data);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_object;
	}

	if (data->state == APPMODE) {
		error = mxt_make_highchg(data);
		if (error) {
			dev_err(&client->dev, "Failed to make high CHG\n");
			goto err_free_irq;
		}
	}

	error = input_register_device(input_dev);
	if (error)
		goto err_free_irq;

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error)
		goto err_unregister_device;

#if defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						MXT_SUSPEND_LEVEL;
	data->early_suspend.suspend = mxt_early_suspend;
	data->early_suspend.resume = mxt_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	mxt_debugfs_init(data);

	mxt_do_force_calibration(data);

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

	mxt_handle_extra_touchdata(data, MXT_REFERENCE_DATA);
	return 0;

err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_unregister_device:
	input_unregister_device(input_dev);
	input_dev = NULL;
err_free_irq:
	free_irq(client->irq, data);
err_free_object:
	kfree(data->object_table);
err_reset_gpio_req:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
err_irq_gpio_req:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_power_on:
	if (pdata->power_on)
		pdata->power_on(false);
	else
		mxt_power_on(data, false);
err_regulator_on:
	if (pdata->init_hw)
		pdata->init_hw(false);
	else
		mxt_regulator_configure(data, false);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
	return error;
}

static int __devexit mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	cancel_delayed_work_sync(&data->force_calibrate_delayed_work);
	cancel_delayed_work_sync(&data->disable_antipalm_delayed_work);
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	if (data->pdata->power_on)
		data->pdata->power_on(false);
	else
		mxt_power_on(data, false);

	if (data->pdata->init_hw)
		data->pdata->init_hw(false);
	else
		mxt_regulator_configure(data, false);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	kfree(data->object_table);
	kfree(data);

	debugfs_remove_recursive(debug_base);

	return 0;
}

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "mXT224", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= "atmel_mxt_ts",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &mxt_pm_ops,
#endif
	},
	.probe		= mxt_probe,
	.remove		= __devexit_p(mxt_remove),
	.id_table	= mxt_id,
};

module_i2c_driver(mxt_driver);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");
