/*
 *  isa1200.c - Haptic Motor
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *  Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/i2c/isa1200.h>
#include "../staging/android/timed_output.h"

#define ISA1200_HCTRL0		0x30
#define ISA1200_HCTRL1		0x31
#define ISA1200_HCTRL5		0x35

#define ISA1200_HCTRL0_RESET	0x01
#define ISA1200_HCTRL1_RESET	0x4B

#define ISA1200_HCTRL5_VIB_STRT	0xD5
#define ISA1200_HCTRL5_VIB_STOP	0x6B
#define ISA1200_POWER_DOWN_MASK 0x7F
#define ISA1200_OVERDRIVE_LH    0x20
#define ISA1200_OVERDRIVE_EN    0x40

#define PAT_MAX_LEN 256
unsigned char pattern[PAT_MAX_LEN];
unsigned char cdbgstr[PAT_MAX_LEN];

struct isa1200_chip {
	struct i2c_client *client;
	struct isa1200_platform_data *pdata;
	struct pwm_device *pwm;
	struct hrtimer timer;
	struct timed_output_dev dev;
	struct work_struct work;
	spinlock_t lock;
	struct mutex vib_lock;
	unsigned int enable;
	unsigned int period_ns;
	bool is_len_gpio_valid;
	struct regulator **regs;
	bool clk_on;
	u8 hctrl0_val;
	struct clk *pwm_clk;

	struct work_struct pat_work;
	struct workqueue_struct *hap_wq;
	unsigned char *pat;
	int	     pat_len;
	int	     pat_i;
	int	     pat_mode;
};

static int isa1200_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int isa1200_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void isa1200_vib_set(struct isa1200_chip *haptic, int enable)
{
	int rc = 0;

	mutex_lock(&haptic->vib_lock);

	if (enable) {
		/* if hen and len are seperate then enable hen
		 * otherwise set normal mode bit */
		if (haptic->is_len_gpio_valid == true)
			gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 1);
		else {
			rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0,
				haptic->hctrl0_val | ~ISA1200_POWER_DOWN_MASK);
			if (rc < 0) {
				pr_err("%s: i2c write failure\n", __func__);
				goto vib_done;
			}
		}

		if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
			int period_us = haptic->period_ns / 1000;

			rc = pwm_config(haptic->pwm,
				(period_us * haptic->pdata->duty) / 100,
				period_us);
			if (rc < 0) {
				pr_err("%s: pwm_config fail\n", __func__);
				goto chip_dwn;
			}

			rc = pwm_enable(haptic->pwm);
			if (rc < 0) {
				pr_err("%s: pwm_enable fail\n", __func__);
				goto chip_dwn;
			}
			gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 1);
		} else if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
			/* check for board specific clk callback */
			if (haptic->pdata->clk_enable) {
				rc = haptic->pdata->clk_enable(true);
				if (rc < 0) {
					pr_err("%s: clk enable cb failed\n",
								__func__);
					goto chip_dwn;
				}
			}

			/* vote for clock */
			if (haptic->pdata->need_pwm_clk && !haptic->clk_on) {
				rc = clk_enable(haptic->pwm_clk);
				if (rc < 0) {
					pr_err("%s: clk enable failed\n",
								__func__);
					goto dis_clk_cb;
				}
				haptic->clk_on = true;
			}

			rc = isa1200_write_reg(haptic->client,
						ISA1200_HCTRL5,
						ISA1200_HCTRL5_VIB_STRT);
			if (rc < 0) {
				pr_err("%s: start vibartion fail\n", __func__);
				goto dis_clk;
			}
		}
	} else {
		/* if hen and len are seperate then pull down hen
		 * otherwise set power down bit */
		if (haptic->is_len_gpio_valid == true)
			gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
		else {
			rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0,
				haptic->hctrl0_val & ISA1200_POWER_DOWN_MASK);
			if (rc < 0) {
				pr_err("%s: i2c write failure\n", __func__);
				goto vib_done;
			}
		}

		if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
			pwm_disable(haptic->pwm);
		} else if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
			rc = isa1200_write_reg(haptic->client,
						ISA1200_HCTRL5,
						ISA1200_HCTRL5_VIB_STOP);
			if (rc < 0)
				pr_err("%s: stop vibartion fail\n", __func__);

			/* de-vote clock */
			if (haptic->pdata->need_pwm_clk && haptic->clk_on) {
				clk_disable(haptic->pwm_clk);
				haptic->clk_on = false;
			}
			/* check for board specific clk callback */
			if (haptic->pdata->clk_enable) {
				rc = haptic->pdata->clk_enable(false);
				if (rc < 0)
					pr_err("%s: clk disable cb failed\n",
								__func__);
			}
		}
	}

	goto vib_done;

dis_clk:
	if (haptic->pdata->need_pwm_clk && haptic->clk_on) {
		clk_disable(haptic->pwm_clk);
		haptic->clk_on = false;
		pr_err("%s: dis_clk\n", __func__);
	}

dis_clk_cb:
	if (haptic->pdata->clk_enable) {
		rc = haptic->pdata->clk_enable(false);
		if (rc < 0)
			pr_err("%s: clk disable cb failed\n", __func__);
	}

chip_dwn:
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	else {
		rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0,
			haptic->hctrl0_val & ISA1200_POWER_DOWN_MASK);
		if (rc < 0) {
			pr_err("%s: i2c write failure\n", __func__);
		}
	}
vib_done:
	mutex_unlock(&haptic->vib_lock);
}

static void isa1200_vib_set_pat(struct isa1200_chip *haptic, int level)
{
	int rc = 0;
	unsigned int hctrl0 = haptic->hctrl0_val;
	int period_us = haptic->period_ns / 1000;

	if  (level != 0) {

		if (level > 0)
			hctrl0 |= ISA1200_OVERDRIVE_LH;
		else
			hctrl0 &= ~ISA1200_OVERDRIVE_LH;

		if (abs(level) > 125)
			hctrl0 |= ISA1200_OVERDRIVE_EN;
		else
			hctrl0 &= ~ISA1200_OVERDRIVE_EN;

		rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0, hctrl0);
		if (rc < 0) {
			pr_err("%s: i2c write failure\n", __func__);
			return;
		}

		rc = pwm_config(haptic->pwm,
				(period_us * (level + 128)) / 256,
				period_us);
		if (rc < 0) {
			pr_err("%s: pwm_config fail\n", __func__);
			goto chip_dwn;
		}

		rc = pwm_enable(haptic->pwm);
		if (rc < 0) {
			pr_err("%s: pwm_enable fail\n", __func__);
			goto chip_dwn;
		}
		if (haptic->is_len_gpio_valid == true)
			gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 1);
	} else {
		if (haptic->is_len_gpio_valid == true)
			gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);

		pwm_disable(haptic->pwm);

		/* restore HCTRL0 register at disable */
		rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0,
				haptic->hctrl0_val);
		if (rc < 0) {
			pr_err("%s: i2c write failure\n", __func__);
			return;
		}

	}

	return;

chip_dwn:
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	else {
		rc = isa1200_write_reg(haptic->client, ISA1200_HCTRL0,
				haptic->hctrl0_val & ISA1200_POWER_DOWN_MASK);
		if (rc < 0) {
			pr_err("%s: i2c write failure\n", __func__);
			return;
		}
	}
}


static void isa1200_chip_work(struct work_struct *work)
{
	struct isa1200_chip *haptic;

	haptic = container_of(work, struct isa1200_chip, work);
	isa1200_vib_set(haptic, haptic->enable);
}

static void isa1200_pat_work(struct work_struct *work)
{
	struct isa1200_chip *haptic;
	int i;
	int level;

	haptic = container_of(work, struct isa1200_chip, pat_work);
	for (i = 1; i < haptic->pat_len; i += 2) {
		level = (signed char)haptic->pat[i];
		isa1200_vib_set_pat(haptic, level);
		pr_debug("%s: vib:%d time:%d", __func__, level, haptic->pat[i + 1]);
		msleep(haptic->pat[i+1]);
	}
}

static void isa1200_chip_enable(struct timed_output_dev *dev, int value)
{
	struct isa1200_chip *haptic = container_of(dev, struct isa1200_chip,
					dev);
	unsigned long flags;

	spin_lock_irqsave(&haptic->lock, flags);
	hrtimer_cancel(&haptic->timer);
	if (value == 0)
		haptic->enable = 0;
	else {
		value = (value > haptic->pdata->max_timeout ?
				haptic->pdata->max_timeout : value);
		haptic->enable = 1;
		hrtimer_start(&haptic->timer,
			ktime_set(value / 1000, (value % 1000) * 1000000),
			HRTIMER_MODE_REL);
	}
	schedule_work(&haptic->work);
	spin_unlock_irqrestore(&haptic->lock, flags);
}

static int isa1200_chip_get_time(struct timed_output_dev *dev)
{
	struct isa1200_chip *haptic = container_of(dev, struct isa1200_chip,
					dev);

	if (hrtimer_active(&haptic->timer)) {
		ktime_t r = hrtimer_get_remaining(&haptic->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static enum hrtimer_restart isa1200_vib_timer_func(struct hrtimer *timer)
{
	struct isa1200_chip *haptic = container_of(timer, struct isa1200_chip,
					timer);
	haptic->enable = 0;
	schedule_work(&haptic->work);

	return HRTIMER_NORESTART;
}

static void dump_isa1200_reg(char *str, struct i2c_client *client)
{
	pr_debug("%s 0x%x   0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", str,
		isa1200_read_reg(client, 0x00),
		isa1200_read_reg(client, ISA1200_HCTRL0),
		isa1200_read_reg(client, ISA1200_HCTRL1),
		isa1200_read_reg(client, 0x32),
		isa1200_read_reg(client, 0x33),
		isa1200_read_reg(client, 0x34),
		isa1200_read_reg(client, 0x35));
}

static int isa1200_setup(struct i2c_client *client)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);
	int temp, rc;
	u8 value;

	if (haptic->is_len_gpio_valid == true) {
		gpio_set_value_cansleep(haptic->pdata->hap_len_gpio, 1);
		udelay(250);
	}

	value =	(haptic->pdata->smart_en << 3) |
		(haptic->pdata->is_erm << 5) |
		(haptic->pdata->ext_clk_en << 7);

	rc = isa1200_write_reg(client, ISA1200_HCTRL1, value);
	if (rc < 0) {
		pr_err("%s: i2c write failure\n", __func__);
		goto reset_gpios;
	}

	if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
		temp = haptic->pdata->pwm_fd.pwm_div;
		if (temp < 128 || temp > 1024 || temp % 128) {
			pr_err("%s: Invalid divider\n", __func__);
			goto reset_hctrl1;
		}
		value = ((temp >> 7) - 1);
	} else if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
		temp = haptic->pdata->pwm_fd.pwm_freq;
		if (temp < 22400 || temp > 172600 || temp % 22400) {
			pr_err("%s: Invalid frequency\n", __func__);
			goto reset_hctrl1;
		}
		value = ((temp / 22400) - 1);
		haptic->period_ns = NSEC_PER_SEC / temp;
	}

	value |= (haptic->pdata->mode_ctrl << 3) |
		(haptic->pdata->overdrive_high << 5) |
		(haptic->pdata->overdrive_en << 6) |
		(haptic->pdata->chip_en << 7);

	rc = isa1200_write_reg(client, ISA1200_HCTRL0, value);
	if (rc < 0) {
		pr_err("%s: i2c write failure\n", __func__);
		goto reset_hctrl1;
	}

	/* if hen and len are seperate then pull down hen
	 * otherwise set power down bit */
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	else {
		rc = isa1200_write_reg(client, ISA1200_HCTRL0,
					value & ISA1200_POWER_DOWN_MASK);
		if (rc < 0) {
			pr_err("%s: i2c write failure\n", __func__);
			goto reset_hctrl1;
		}
	}

	/* set max voltage to 3.2v LDO output should be 3.0v */
	rc = isa1200_write_reg(client, 0x00, 0x01);
	if (rc < 0) {
		pr_err("%s: i2c write failure\n", __func__);
		goto reset_hctrl1;
	}

	haptic->hctrl0_val = value;
	dump_isa1200_reg("isa1200 new:", client);
	return 0;

reset_hctrl1:
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL1,
				ISA1200_HCTRL1_RESET);
reset_gpios:
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_len_gpio, 0);
	return rc;
}

static int isa1200_reg_power(struct isa1200_chip *haptic, bool on)
{
	const struct isa1200_regulator *reg_info =
				haptic->pdata->regulator_info;
	u8 i, num_reg = haptic->pdata->num_regulators;
	int rc;

	for (i = 0; i < num_reg; i++) {
		rc = regulator_set_optimum_mode(haptic->regs[i],
					on ? reg_info[i].load_uA : 0);
		if (rc < 0) {
			pr_err("%s: regulator_set_optimum_mode failed(%d)\n",
							__func__, rc);
			goto regs_fail;
		}

		rc = on ? regulator_enable(haptic->regs[i]) :
			regulator_disable(haptic->regs[i]);
		if (rc < 0) {
			pr_err("%s: regulator %sable fail %d\n", __func__,
					on ? "en" : "dis", rc);
			regulator_set_optimum_mode(haptic->regs[i],
					!on ? reg_info[i].load_uA : 0);
			goto regs_fail;
		}
	}

	return 0;

regs_fail:
	while (i--) {
		regulator_set_optimum_mode(haptic->regs[i],
				!on ? reg_info[i].load_uA : 0);
		!on ? regulator_enable(haptic->regs[i]) :
			regulator_disable(haptic->regs[i]);
	}
	return rc;
}

static int isa1200_reg_setup(struct isa1200_chip *haptic, bool on)
{
	const struct isa1200_regulator *reg_info =
				haptic->pdata->regulator_info;
	u8 i, num_reg = haptic->pdata->num_regulators;
	int rc = 0;

	/* put regulators */
	if (on == false) {
		i = num_reg;
		goto put_regs;
	}

	haptic->regs = kzalloc(num_reg * sizeof(struct regulator *),
							GFP_KERNEL);
	if (!haptic->regs) {
		pr_err("unable to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_reg; i++) {
		haptic->regs[i] = regulator_get(&haptic->client->dev,
							reg_info[i].name);
		if (IS_ERR(haptic->regs[i])) {
			rc = PTR_ERR(haptic->regs[i]);
			pr_err("%s:regulator get failed(%d)\n",	__func__, rc);
			goto put_regs;
		}

		if (regulator_count_voltages(haptic->regs[i]) > 0) {
			rc = regulator_set_voltage(haptic->regs[i],
				reg_info[i].min_uV, reg_info[i].max_uV);
			if (rc) {
				pr_err("%s: regulator_set_voltage failed(%d)\n",
								__func__, rc);
				regulator_put(haptic->regs[i]);
				goto put_regs;
			}
		}
	}

	return rc;

put_regs:
	while (i--) {
		if (regulator_count_voltages(haptic->regs[i]) > 0)
			regulator_set_voltage(haptic->regs[i], 0,
						reg_info[i].max_uV);
		regulator_put(haptic->regs[i]);
	}
	kfree(haptic->regs);
	return rc;
}

static ssize_t isa1200_write_pattern(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buffer, loff_t offset, size_t count)
{
	unsigned long flags;
	int time;
	struct timed_output_dev *tdev;
	struct isa1200_chip *haptic;

	tdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	haptic = container_of(tdev, struct isa1200_chip, dev);

	spin_lock_irqsave(&haptic->lock, flags);

	memcpy(pattern, buffer, count);
	time = pattern[2];
	pattern[count] = 0;
	pattern[count + 1] = 10;
	haptic->pat_mode = pattern[0];
	haptic->pat_len = count + 2;
	haptic->pat_i = 1;

	time = (time > haptic->pdata->max_timeout ?
			haptic->pdata->max_timeout : time);

	queue_work(haptic->hap_wq, &haptic->pat_work);

	spin_unlock_irqrestore(&haptic->lock, flags);

	return 0;
}

static ssize_t isa1200_read_cdbgstr(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buffer, loff_t off, size_t count)
{
	struct timed_output_dev *tdev;
	struct isa1200_chip *haptic;
	tdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	haptic = container_of(tdev, struct isa1200_chip, dev);

	if (count > 256)
		count = 256;
	memcpy(buffer, cdbgstr, count);
	pr_debug("isa1200 read cdbg:%s", cdbgstr);
	return count;
}

static ssize_t isa1200_write_cdbgstr(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buffer, loff_t offset, size_t count)
{
	struct timed_output_dev *tdev;
	struct isa1200_chip *haptic;
	tdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	haptic = container_of(tdev, struct isa1200_chip, dev);

	if (count > 256)
		count = 256;
	memcpy(cdbgstr, buffer, count);
	pr_debug("isa1200 write cdbg:%s", cdbgstr);
	return count;
}

static struct bin_attribute isa1200_bin_attrs = {
	.attr   = {
		.name   = "pattern",
		.mode   = 0644
	},
	.write  = isa1200_write_pattern,
	.size   = PAT_MAX_LEN + 1,
};

static struct bin_attribute isa1201_bin_attrs = {
	.attr   = {
		.name   = "cdbgstr",
		.mode   = 0644
	},
	.read   =  isa1200_read_cdbgstr,
	.size   =  PAT_MAX_LEN + 1,
};

static struct bin_attribute isa1202_bin_attrs = {
	.attr   = {
		.name   = "cdbgwrt",
		.mode   = 0644
	},
	.write  =  isa1200_write_cdbgstr,
	.size   =  PAT_MAX_LEN + 1,
};

static int __devinit isa1200_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isa1200_chip *haptic;
	struct isa1200_platform_data *pdata;
	int ret;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "%s: no support for i2c read/write"
				"byte data\n", __func__);
		return -EIO;
	}

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -EINVAL;
	}

	if (pdata->dev_setup) {
		ret = pdata->dev_setup(true);
		if (ret < 0) {
			dev_err(&client->dev, "dev setup failed\n");
			return -EINVAL;
		}
	}

	haptic = kzalloc(sizeof(struct isa1200_chip), GFP_KERNEL);
	if (!haptic) {
		ret = -ENOMEM;
		goto mem_alloc_fail;
	}
	haptic->client = client;
	haptic->enable = 0;
	haptic->pdata = pdata;

	if (pdata->regulator_info) {
		ret = isa1200_reg_setup(haptic, true);
		if (ret) {
			dev_err(&client->dev, "%s: regulator setup failed\n",
							__func__);
			goto reg_setup_fail;
		}

		ret = isa1200_reg_power(haptic, true);
		if (ret) {
			dev_err(&client->dev, "%s: regulator power failed\n",
							__func__);
			goto reg_pwr_fail;
		}
	}

	if (pdata->power_on) {
		ret = pdata->power_on(1);
		if (ret) {
			dev_err(&client->dev, "%s: power-up failed\n",
							__func__);
			goto pwr_up_fail;
		}
	}

	spin_lock_init(&haptic->lock);
	mutex_init(&haptic->vib_lock);
	INIT_WORK(&haptic->work, isa1200_chip_work);
	INIT_WORK(&haptic->pat_work, isa1200_pat_work);
	haptic->clk_on = false;

	hrtimer_init(&haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	haptic->timer.function = isa1200_vib_timer_func;

	/*register with timed output class*/
	haptic->dev.name = pdata->name;
	haptic->dev.get_time = isa1200_chip_get_time;
	haptic->dev.enable = isa1200_chip_enable;
	ret = timed_output_dev_register(&haptic->dev);
	if (ret < 0)
		goto timed_reg_fail;

	i2c_set_clientdata(client, haptic);

	ret = gpio_is_valid(pdata->hap_en_gpio);
	if (ret) {
		ret = gpio_request(pdata->hap_en_gpio, "haptic_en_gpio");
		if (ret) {
			dev_err(&client->dev, "%s: gpio %d request failed\n",
					__func__, pdata->hap_en_gpio);
			goto hen_gpio_fail;
		}
	} else {
		dev_err(&client->dev, "%s: Invalid gpio %d\n", __func__,
					pdata->hap_en_gpio);
		goto hen_gpio_fail;
	}

	haptic->is_len_gpio_valid = true;
	ret = gpio_is_valid(haptic->pdata->hap_len_gpio);
	if (ret) {
		ret = gpio_request(pdata->hap_len_gpio,
					"haptic_ldo_gpio");
		if (ret) {
			dev_err(&client->dev,
				"%s: gpio %d request failed\n",
				__func__, pdata->hap_len_gpio);
			goto len_gpio_fail;
		}
	} else {
		dev_err(&client->dev, "%s: gpio is not used/Invalid %d\n",
					__func__, pdata->hap_len_gpio);
		haptic->is_len_gpio_valid = false;
	}

	ret = isa1200_setup(client);
	if (ret) {
		dev_err(&client->dev, "%s: setup fail %d\n", __func__, ret);
		goto setup_fail;
	}

	if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
		haptic->pwm = pwm_request(pdata->pwm_ch_id, id->name);
		if (IS_ERR(haptic->pwm)) {
			dev_err(&client->dev, "%s: pwm request failed\n",
							__func__);
			ret = PTR_ERR(haptic->pwm);
			goto reset_hctrl0;
		}
	} else if (haptic->pdata->need_pwm_clk) {
		haptic->pwm_clk = clk_get(&client->dev, "pwm_clk");
		if (IS_ERR(haptic->pwm_clk)) {
			dev_err(&client->dev, "pwm_clk get failed\n");
			ret = PTR_ERR(haptic->pwm_clk);
			goto reset_hctrl0;
		}
	}

	/* Create binary file */
	ret = device_create_bin_file(haptic->dev.dev, &isa1200_bin_attrs);
	ret = device_create_bin_file(haptic->dev.dev, &isa1201_bin_attrs);
	ret = device_create_bin_file(haptic->dev.dev, &isa1202_bin_attrs);
	if (ret)
		goto reset_hctrl0;

	haptic->hap_wq = alloc_workqueue("haptic_wq", WQ_HIGHPRI, 0);
	if (haptic->hap_wq  == NULL) {
		ret = -ENOMEM;
		goto err_create_workqueue;
	}

	haptic->pat = pattern;
	printk(KERN_INFO "%s: %s registered\n", __func__, id->name);
	return 0;

err_create_workqueue:
	destroy_workqueue(haptic->hap_wq);
reset_hctrl0:
	gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_len_gpio, 0);
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL1,
				ISA1200_HCTRL1_RESET);
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL0,
					ISA1200_HCTRL0_RESET);
setup_fail:
	if (haptic->is_len_gpio_valid == true)
		gpio_free(pdata->hap_len_gpio);
len_gpio_fail:
	gpio_free(pdata->hap_en_gpio);
hen_gpio_fail:
	timed_output_dev_unregister(&haptic->dev);
timed_reg_fail:
	if (pdata->power_on)
		pdata->power_on(0);
pwr_up_fail:
	if (pdata->regulator_info)
		isa1200_reg_power(haptic, false);
reg_pwr_fail:
	if (pdata->regulator_info)
		isa1200_reg_setup(haptic, false);
reg_setup_fail:
	kfree(haptic);
mem_alloc_fail:
	if (pdata->dev_setup)
		pdata->dev_setup(false);
	return ret;
}

static int __devexit isa1200_remove(struct i2c_client *client)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);

	hrtimer_cancel(&haptic->timer);
	cancel_work_sync(&haptic->work);
	cancel_work_sync(&haptic->pat_work);
	destroy_workqueue(haptic->hap_wq);

	/* turn-off current vibration */
	isa1200_vib_set(haptic, 0);

	if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE)
		pwm_free(haptic->pwm);

	timed_output_dev_unregister(&haptic->dev);

	gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_len_gpio, 0);

	gpio_free(haptic->pdata->hap_en_gpio);
	if (haptic->is_len_gpio_valid == true)
		gpio_free(haptic->pdata->hap_len_gpio);

	/* reset hardware registers */
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL0,
				ISA1200_HCTRL0_RESET);
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL1,
				ISA1200_HCTRL1_RESET);


	/* power-off the chip */
	if (haptic->pdata->regulator_info) {
		isa1200_reg_power(haptic, false);
		isa1200_reg_setup(haptic, false);
	}

	if (haptic->pdata->power_on)
		haptic->pdata->power_on(0);

	if (haptic->pdata->dev_setup)
		haptic->pdata->dev_setup(false);

	kfree(haptic);
	return 0;
}

#ifdef CONFIG_PM
static int isa1200_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);
	int ret;

	hrtimer_cancel(&haptic->timer);
	cancel_work_sync(&haptic->work);
	cancel_work_sync(&haptic->pat_work);
	/* turn-off current vibration */
	isa1200_vib_set(haptic, 0);

	if (haptic->is_len_gpio_valid == true)
		gpio_set_value_cansleep(haptic->pdata->hap_len_gpio, 0);

	if (haptic->pdata->regulator_info)
		isa1200_reg_power(haptic, false);

	if (haptic->pdata->power_on) {
		ret = haptic->pdata->power_on(0);
		if (ret) {
			dev_err(&client->dev, "power-down failed\n");
			return ret;
		}
	}

	return 0;
}

static int isa1200_resume(struct i2c_client *client)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);
	int ret;

	if (haptic->pdata->regulator_info)
		isa1200_reg_power(haptic, true);

	if (haptic->pdata->power_on) {
		ret = haptic->pdata->power_on(1);
		if (ret) {
			dev_err(&client->dev, "power-up failed\n");
			return ret;
		}
	}

	isa1200_setup(client);
	return 0;
}
#else
#define isa1200_suspend		NULL
#define isa1200_resume		NULL
#endif

static const struct i2c_device_id isa1200_id[] = {
	{ "isa1200_1", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isa1200_id);

static struct i2c_driver isa1200_driver = {
	.driver	= {
		.name	= "isa1200",
	},
	.probe		= isa1200_probe,
	.remove		= __devexit_p(isa1200_remove),
	.suspend	= isa1200_suspend,
	.resume		= isa1200_resume,
	.id_table	= isa1200_id,
};

static int __init isa1200_init(void)
{
	return i2c_add_driver(&isa1200_driver);
}

static void __exit isa1200_exit(void)
{
	i2c_del_driver(&isa1200_driver);
}

module_init(isa1200_init);
module_exit(isa1200_exit);

MODULE_AUTHOR("Kyungmin Park <kyungmin.park@samsung.com>");
MODULE_DESCRIPTION("ISA1200 Haptic Motor driver");
MODULE_LICENSE("GPL");
