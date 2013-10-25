/* es310 voice processor reference driver
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/kernel.h>
#include <linux/serial_core.h>
#include <linux/clk.h>

#include <sound/es310.h>

#define DEBUG_I2C_BUS_TRANSFER_DUMP 0

static int es310_suspended;
static int es310_current_config = ES310_PATH_SUSPEND;
static int es310_current_preset = ES310_PRESET_AUDIOPATH_DISABLE;
static char *config_data;

static char default_config_data[] = {
	ES310_PATH_HANDSET, 0x80, 0x26, 0x00, 0x43,
	ES310_PATH_HEADSET, 0x80, 0x26, 0x00, 0x49,
	ES310_PATH_HANDSFREE, 0x80, 0x26, 0x00, 0x43,
	ES310_PATH_BACKMIC, 0x80, 0x26, 0x00, 0x49,
};

static int es310_cmds_len;

static bool ES310_SYNC_DONE;

#define PARAM_MAX	(sizeof(char) * 6 * 1024)

static struct i2c_client *this_client;
static struct es310_platform_data *pdata;

static void es310_gpio_set_value(int gpio, int value);

static int es310_i2c_write(char *txData, int length);

static int es310_i2c_read(char *rxData, int length);
int execute_cmdmsg(unsigned int msg);
static int es310_wakeup(void);

struct vp_ctxt {
	unsigned char *data;
	unsigned int img_size;
};

struct vp_ctxt the_vp;

static int mic_switch_table[ES310_PATH_MAX + 1];

static unsigned char ES310_CMD_ROUTE[28][4]  = {
{0x80, 0x0C, 0x0A, 0x00},
{0x80, 0x0D, 0x00, 0x0F},
{0x80, 0x0C, 0x0A, 0x02},
{0x80, 0x0D, 0x00, 0x00},
{0x80, 0x0C, 0x0A, 0x03},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0A, 0x04},
{0x80, 0x0D, 0x00, 0x00},
{0x80, 0x0C, 0x0A, 0x05},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0A, 0x06},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0A, 0x07},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0C, 0x00},
{0x80, 0x0D, 0x00, 0x0F},
{0x80, 0x0C, 0x0C, 0x02},
{0x80, 0x0D, 0x00, 0x00},
{0x80, 0x0C, 0x0C, 0x03},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0C, 0x04},
{0x80, 0x0D, 0x00, 0x00},
{0x80, 0x0C, 0x0C, 0x05},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0C, 0x06},
{0x80, 0x0D, 0x00, 0x01},
{0x80, 0x0C, 0x0C, 0x07},
{0x80, 0x0D, 0x00, 0x01}
};

static struct clk *anc_mclk;

static void es310_i2c_sw_reset(unsigned int reset_cmd)
{
	int rc = 0;
	unsigned char msgbuf[4];

	msgbuf[0] = (reset_cmd >> 24) & 0xFF;
	msgbuf[1] = (reset_cmd >> 16) & 0xFF;
	msgbuf[2] = (reset_cmd >> 8) & 0xFF;
	msgbuf[3] = reset_cmd & 0xFF;

	rc = es310_i2c_write(msgbuf, 4);
	if (!rc)
		msleep(30);
}

static int es310_hardreset(void)
{
	int rc = 0;

	rc = es310_wakeup();
	if (rc < 0) {
		pr_err("%s: ES310 wakeup fail", __func__);
		return rc;
	}

	rc = gpio_direction_output(pdata->gpio_es310_reset, 0);
	if (rc < 0) {
		pr_err("%s: request reset gpio direction failed\n", __func__);
		goto err_free_gpio;
	}
	mdelay(1);
	es310_gpio_set_value(pdata->gpio_es310_reset, 1);
	mdelay(50);
	return 0;

err_free_gpio:
	return -1;
}

static int es310_wakeup(void)
{
	int rc = 0, retry = 3;

	if (!es310_suspended)
		return 0;

	clk_prepare(anc_mclk);
	rc = clk_enable(anc_mclk);
	if (rc) {
		pr_err("%s: enable clk failed\n", __func__);
		return rc;
	}
	msleep(5);
	rc = gpio_request(pdata->gpio_es310_wakeup, "es310_wakeup_pin");
	if (rc < 0) {
		pr_err("%s: request wakeup gpio failed\n", __func__);
		goto wakeup_disable_clk;
	}
	rc = gpio_direction_output(pdata->gpio_es310_wakeup, 1);
	if (rc < 0) {
		pr_err("%s: set wakeup gpio to HIGH failed\n", __func__);
		goto wakeup_free_gpio;
	}
	msleep(1);
	rc = gpio_direction_output(pdata->gpio_es310_wakeup, 0);
	if (rc < 0) {
		pr_err("%s: set wakeup gpio to LOW failed\n", __func__);
		goto wakeup_free_gpio;
	}
	msleep(40);

	rc = gpio_direction_output(pdata->gpio_es310_wakeup, 1);
	if (rc < 0) {
		pr_err("%s: set wakeup gpio to HIGH failed\n", __func__);
		goto wakeup_free_gpio;
	}

	do {
		rc = execute_cmdmsg(A200_msg_Sync_Polling);
	} while ((rc < 0) && --retry);

	if ((rc < 0) || (retry == 0))
		pr_err("%s: es310 wakeup failed (%d)\n", __func__, rc);

	es310_suspended = 0;

	gpio_free(pdata->gpio_es310_wakeup);
	return rc;

wakeup_free_gpio:
	gpio_free(pdata->gpio_es310_wakeup);
wakeup_disable_clk:
	clk_disable(anc_mclk);
	clk_unprepare(anc_mclk);

	return rc;
}

int es310_sleep(void)
{
	int64_t t1, t2;
	int rc = 0;

	if (es310_suspended == 1) {
		pr_debug("%s es310 already suspended\n", __func__);
		return rc;
	}

	t1 = ktime_to_ms(ktime_get());

	rc = execute_cmdmsg(A200_msg_SetPowerState_Sleep);
	if (rc < 0) {
		pr_err("%s: suspend error\n", __func__);
		return -1;
	}

	es310_suspended = 1;
	es310_current_config = ES310_PATH_SUSPEND;

	msleep(30);
	clk_disable(anc_mclk);
	clk_unprepare(anc_mclk);

	t2 = ktime_to_ms(ktime_get()) - t1;

	return rc;
}

int es310_port_config(void)
{
	int i;

	for (i = 0; i < 28; i++) {
		es310_i2c_write(ES310_CMD_ROUTE[i], 4);
		mdelay(20);

	}
	return 0;
}

int es310_set_preset(unsigned int preset_mode)
{
	int rc = 0;

	if (es310_suspended) {
		pr_info("ES310 suspended, wakeup it");
		rc = es310_wakeup();
		if (rc < 0) {
			pr_debug("ES310 set path fail, wakeup fail");
			return rc;
		}
	}

	pr_info("%s: Set preset mode: 0x%x", __func__, preset_mode);
	rc = execute_cmdmsg(preset_mode);
	if (rc == 0)
		es310_current_preset = preset_mode;
	else
		pr_err("%s: Set preset mode 0x%x failed\n", __func__, preset_mode);

	return rc;
}

int build_cmds(char *cmds, int newid)
{
	int i = 0;
	int offset = 0;
	char *config;
	int length;
	int match_found = 0;

	if (config_data) {
		config = config_data;
		length = es310_cmds_len;
	} else {
		config = default_config_data;
		length = sizeof(default_config_data);
	}

	for (i = 0; (i + 4) < length; i += 5) {
		if (config[i] == newid) {
			match_found = 1;
			cmds[offset++] = config[i + 1];
			cmds[offset++] = config[i + 2];
			cmds[offset++] = config[i + 3];
			cmds[offset++] = config[i + 4];
		}
	}
	if (!match_found)
		pr_warn("%s: no match found in config table, newid=%d\n",
			__func__, newid);

	return offset;
}

static unsigned int DEBUG_STRESS_TEST_COUNT = 1;

int es310_set_config(int newid, int mode)
{
	int block_size = 128;

	int rc = 0, size = 0;
	unsigned int sw_reset = 0;
	unsigned char *i2c_cmds;
	unsigned char ack_buf[ES310_CMD_FIFO_DEPTH * 4];
	unsigned char custom_cmds[800] = {0};
	int cmd_size;
	int remaining;
	int pass;
	int remainder;
	unsigned char *ptr;

	if (es310_suspended) {
		rc = es310_wakeup();
		if (rc < 0) {
			pr_debug("ES310 set path fail, wakeup fail");
			return rc;
		}
	}

	sw_reset = ((A200_msg_Reset << 16) | RESET_IMMEDIATE);

	es310_current_config = newid;
	cmd_size = build_cmds(custom_cmds, newid);
	if (cmd_size > 0)
		i2c_cmds = custom_cmds;
	size = cmd_size;

	remaining = size;
	pass = size / block_size;
	remainder = size % block_size;
	ptr = i2c_cmds;

	while (pass) {
		rc = es310_i2c_write(ptr, block_size);
		if (rc < 0) {
			pr_err("ES310 CMD block write error!\n");
			es310_i2c_sw_reset(sw_reset);
			return rc;
		}
		ptr += block_size;
		pass--;

		mdelay(20);
		memset(ack_buf, 0, sizeof(ack_buf));
		rc = es310_i2c_read(ack_buf, block_size);
		if (rc < 0) {
			pr_err("%s: CMD ACK block read error\n", __func__);
			DEBUG_STRESS_TEST_COUNT++;
			es310_i2c_sw_reset(sw_reset);
			return rc;
		} else {
			if (*ack_buf != 0x80) {
				pr_err("%s: CMD ACK fail, ES310 may be died\n", __func__);
				DEBUG_STRESS_TEST_COUNT++;
				es310_i2c_sw_reset(sw_reset);
				return -1;
			}
		}
	}

	if (remainder) {
		rc = es310_i2c_write(ptr, remainder);
		if (rc < 0) {
			pr_err("ES310 CMD block write error!\n");

			es310_i2c_sw_reset(sw_reset);
			return rc;
		}

		mdelay(20);
		memset(ack_buf, 0, sizeof(ack_buf));
		rc = es310_i2c_read(ack_buf, remainder);
		if (rc < 0) {
			pr_err("%s: CMD ACK block read error\n", __func__);
			DEBUG_STRESS_TEST_COUNT++;
			es310_i2c_sw_reset(sw_reset);
			return rc;
		} else {
			if (*ack_buf != 0x80) {
				pr_err("%s: CMD ACK fail, ES310 may be died\n", __func__);
				DEBUG_STRESS_TEST_COUNT++;
				es310_i2c_sw_reset(sw_reset);
				return -1;
			}
		}
	}
	return rc;
}

static int setup_mic_switch(int sw)
{
	int rc = 0;

	if (sw == MIC_SWITCH_AUXILIARY_MIC) {
		rc = gpio_direction_output(pdata->gpio_es310_mic_switch, 1);
		if (rc < 0)
			pr_err("%s: set switch gpio to HIGH failed\n", __func__);
	} else if (sw == MIC_SWITCH_HEADSET_MIC) {
		rc = gpio_direction_output(pdata->gpio_es310_mic_switch, 0);
		if (rc < 0)
			pr_err("%s: set switch gpio to LOW failed\n", __func__);
	}
	msleep(5);

	return rc;
}

static long es310_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct ES310_config_data cfg;
	int rc = 0;

	char msg[4];

	unsigned int pathid;
	unsigned int preset_mode;

	switch (cmd) {
	case ES310_RESET_CMD:
		rc = es310_hardreset();
		break;
	case ES310_SLEEP_CMD:
		rc = es310_sleep();
		break;
	case ES310_WAKEUP_CMD:
		rc = es310_wakeup();
		break;
	case ES310_SYNC_CMD:
		mdelay(120);
		rc = execute_cmdmsg(A200_msg_Sync_Polling);
		if (rc < 0) {
			pr_err("%s: sync command error %d", __func__, rc);
		} else {
			es310_port_config();
			if (es310_sleep() < 0) {
				pr_err("%s: sleep command fail", __func__);
			} else {
				ES310_SYNC_DONE = 1;
				pr_info("%s: sleep command ok", __func__);
			}
		}
		break;
	case ES310_READ_SYNC_DONE:
		if (copy_to_user(argp, &ES310_SYNC_DONE, sizeof(ES310_SYNC_DONE)))
			return -EFAULT;
		break;

	case ES310_MDELAY:
		break;
	case ES310_READ_FAIL_COUNT:
		if (copy_to_user(argp, &DEBUG_STRESS_TEST_COUNT, sizeof(DEBUG_STRESS_TEST_COUNT)))
			return -EFAULT;

		break;

	case ES310_SET_CONFIG:
		if (copy_from_user(&pathid, argp, sizeof(unsigned int))) {
			pr_err("%s: copy from user failed.\n", __func__);
			return -EFAULT;
		}

		if (pathid < 0 || pathid >= (unsigned int)ES310_PATH_MAX) {
			pr_debug("%s: pathid (%d) is invalid\n", __func__, pathid);
			return -EINVAL;
		}

		if (pathid == ES310_PATH_SUSPEND) {
			pr_info("%s start to es310_sleep, id:%d \n", __func__, pathid);
			rc = es310_sleep();
		} else {
			pr_info("%s start to es310_set_config, id:%d\n", __func__, pathid);
			rc = es310_set_config(pathid, ES310_CONFIG_FULL);
			if (!rc) {
				if (mic_switch_table[pathid] !=
				    MIC_SWITCH_UNUSED)
					rc = setup_mic_switch(
						mic_switch_table[pathid]);
			}
		}

		if (rc < 0)
			pr_err("%s: ES310_SET_CONFIG (%d) error %d!\n",
				__func__, pathid, rc);

		break;
	case ES310_SET_PARAM:
		es310_cmds_len = 0;
		cfg.cmd_data = 0;
		if (copy_from_user(&cfg, argp, sizeof(cfg))) {
			pr_err("%s: copy from user failed.\n", __func__);
			return -EFAULT;
		}

		if (cfg.data_len <= 0 || cfg.data_len > PARAM_MAX) {
				pr_err("%s: invalid data length %d\n", \
					__func__,  cfg.data_len);
				return -EINVAL;
		}

		if (cfg.cmd_data == NULL) {
			pr_err("%s: invalid data\n", __func__);
			return -EINVAL;
		}

		if (config_data == NULL)
			config_data = kmalloc(cfg.data_len, GFP_KERNEL);
		if (!config_data) {
			pr_err("%s: out of memory\n", __func__);
			return -ENOMEM;
		}
		if (copy_from_user(config_data, cfg.cmd_data, cfg.data_len)) {
			pr_err("%s: copy data from user failed.\n",\
				__func__);
			kfree(config_data);
			config_data = NULL;
			return -EFAULT;
		}
		es310_cmds_len = cfg.data_len;
		rc = 0;
		break;
	case ES310_READ_DATA:
		rc = es310_wakeup();
		if (rc < 0)
			return rc;
		rc = es310_i2c_read(msg, 4);
		if (copy_to_user(argp, &msg, 4))
			return -EFAULT;
		break;
	case ES310_WRITE_MSG:
		rc = es310_wakeup();
		if (rc < 0)
			return rc;
		if (copy_from_user(msg, argp, sizeof(msg)))
			return -EFAULT;
		rc = es310_i2c_write(msg, 4);
		break;
	case ES310_SET_PRESET:
		if (copy_from_user(&preset_mode, argp, sizeof(unsigned int))) {
			pr_err("%s: copy from user failed.\n", __func__);
			return -EFAULT;
		}

		pr_info("%s current preset:0x%x, new preset:0x%x", __func__,
			es310_current_preset, preset_mode);
		rc = es310_set_preset(preset_mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int es310_open(struct inode *inode, struct file *file)
{
	return 0;
}

int es310_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const struct file_operations es310_fileops = {
	.owner = THIS_MODULE,
	.open = es310_open,
	.unlocked_ioctl = es310_ioctl,
	.release = es310_release,
};

static struct miscdevice es310_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "audience_es310",
	.fops = &es310_fileops,
};

static void es310_gpio_set_value(int gpio, int value)
{
	if (gpio >  0)
		gpio_set_value(gpio, value);
}

static int es310_i2c_read(char *rxData, int length)
{
	int rc;

	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int es310_i2c_write(char *txData, int length)
{
	int rc;

	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	rc = i2c_transfer(this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

int execute_cmdmsg(unsigned int msg)
{
	int rc = 0;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];
	unsigned int sw_reset = 0;

	sw_reset = ((A200_msg_Reset << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	retries = 3;
	while (retries--) {
		rc = 0;

		mdelay(POLLING_TIMEOUT); /* use polling */
		rc = es310_i2c_write(msgbuf, 4);
		if (rc < 0) {
			pr_err("%s: error %d\n", __func__, rc);
			es310_i2c_sw_reset(sw_reset);
			return rc;
		}

		if (msg == A200_msg_SetPowerState_Sleep)
			return rc;

		memset(chkbuf, 0xaa, sizeof(chkbuf));
		rc = es310_i2c_read(chkbuf, 4);
		if (rc < 0) {
			pr_err("%s: ack-read error %d (%d retries)\n",\
				__func__, rc, retries);
			continue;
		}

		if ((msgbuf[0] == chkbuf[0]) && (msgbuf[1] == chkbuf[1]) &&
		    (msgbuf[2] == chkbuf[2]) && (msgbuf[3] == chkbuf[3])) {
			pass = 1;
			pr_debug("Execute_cmd OK, %08x", msg);
			break;
		} else if (msgbuf[2] == 0xff && msgbuf[3] == 0xff) {
			pr_err("%s: illegal cmd %08x, %x, %x, %x, %x\n",
				__func__, msg, chkbuf[0], chkbuf[1],
				chkbuf[2], chkbuf[3]);
			rc = -EINVAL;
			continue;
		} else if (msgbuf[2] == 0x00 && msgbuf[3] == 0x00) {
			pr_err("%s: not ready(%d retries), %x, %x, %x, %x\n",
				__func__, retries, chkbuf[0], chkbuf[1],
				chkbuf[2], chkbuf[3]);
			rc = -EBUSY;
			continue;
		} else {
			pr_debug("%s: cmd/ack mismatch: (%d retries left), " \
				"%x, %x, %x, %x\n", __func__, retries,
				chkbuf[0], chkbuf[1], chkbuf[2], chkbuf[3]);
			rc = -EBUSY;
			continue;
		}
	}

	if (!pass) {
		pr_err("%s: failed execute cmd %08x (%d)\n", __func__,
			msg, rc);
		es310_i2c_sw_reset(sw_reset);
	}
	return rc;
}

static int es310_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int es310_resume(struct i2c_client *client)
{
	return 0;
}

static int es310_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: platform data is NULL\n", __func__);
		goto err_out;
	}
	this_client = client;

	if (pdata->power_setup) {
		rc = pdata->power_setup(1);
		if (rc) {
			pr_err("%s: power setup failed %d\n", __func__, rc);
			goto err_out;
		}
	}

	anc_mclk = clk_get(NULL, "gp0_clk");
	if (IS_ERR(anc_mclk)) {
		pr_err("%s: get anc_mclk failed (%ld)\n", __func__, (signed long)anc_mclk);
		goto err_out;
	} else {
#define ANC_MCLK_RATE 12000000
		pr_debug("%s: setting anc_mclk to %luHz\n", __func__, (unsigned long)ANC_MCLK_RATE);
		clk_set_rate(anc_mclk, ANC_MCLK_RATE);
		clk_prepare(anc_mclk);
		rc = clk_enable(anc_mclk);
		if (rc) {
			pr_err("%s: enable clk failed\n", __func__);
			goto err_out;
		}
	}

	rc = gpio_request(pdata->gpio_es310_reset, "es310 GPIO reset");
	if (rc < 0) {
		pr_err("%s: gpio request reset pin failed\n", __func__);
		goto err_disable_clk;
	}

	rc = gpio_request(pdata->gpio_es310_mic_switch, "voiceproc_mic_switch");
	if (rc < 0) {
		pr_err("%s: request voiceproc mic switch gpio failed\n", __func__);
		goto err_free_gpio_reset;
	}

	rc = es310_hardreset();
	if (rc < 0) {
		pr_err("%s: es310_hardreset error %d", __func__, rc);
		goto err_free_gpio_switch;
	}

	rc = misc_register(&es310_device);
	if (rc) {
		pr_err("%s: es310_device register failed\n", __func__);
		goto err_free_gpio_switch;
	}

	return 0;

err_free_gpio_switch:
	gpio_free(pdata->gpio_es310_mic_switch);
err_free_gpio_reset:
	gpio_free(pdata->gpio_es310_reset);
err_disable_clk:
	clk_disable_unprepare(anc_mclk);
err_out:
	return rc;
}

static int es310_i2c_remove(struct i2c_client *client)
{
	struct es310_platform_data *pes310data = i2c_get_clientdata(client);

	if (pes310data)
		kfree(pes310data);

	if (pdata) {
		gpio_free(pdata->gpio_es310_mic_switch);
		gpio_free(pdata->gpio_es310_reset);
	}

	return 0;
}

static const struct i2c_device_id es310_i2c_id[] = {
	{"audience_es310", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, es310_i2c_id);

static struct i2c_driver es310_i2c_driver = {
	.driver = {
		.name = "vp_audience_es310",
		.owner = THIS_MODULE,
	},
	.probe = es310_i2c_probe,
	.remove = es310_i2c_remove,
	.suspend = es310_suspend,
	.resume = es310_resume,
	.id_table = es310_i2c_id,
};

static int __init es310_init(void)
{
	int ret;

	memset(mic_switch_table, MIC_SWITCH_UNUSED, sizeof(mic_switch_table));

	mic_switch_table[ES310_PATH_HANDSET]	= MIC_SWITCH_AUXILIARY_MIC;
	mic_switch_table[ES310_PATH_HEADSET]	= MIC_SWITCH_HEADSET_MIC;
	mic_switch_table[ES310_PATH_HANDSFREE]	= MIC_SWITCH_AUXILIARY_MIC;
	mic_switch_table[ES310_PATH_BACKMIC]	= MIC_SWITCH_AUXILIARY_MIC;

	ret = i2c_add_driver(&es310_i2c_driver);

	return ret;
}
module_init(es310_init);

static void __exit es310_deinit(void)
{
	i2c_del_driver(&es310_i2c_driver);
}
module_exit(es310_deinit);
