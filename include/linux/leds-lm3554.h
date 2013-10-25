/*
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef _LINUX_LED_FLASH_LM3554_H__
#define _LINUX_LED_FLASH_LM3554_H__

#define LM3554_LED_DEV "torch-flash"
#define LM3554_LED_SPOTLIGHT "flashlight"

#define LM3554_NAME "lm3554"



#ifdef __KERNEL__

struct lm3554_platform_data {
	u8 torch_brightness_def;
	u8 flash_brightness_def;
	u8 flash_duration_def;
	u8 config_reg_1_def;
	u8 config_reg_2_def;
	u8 vin_monitor_def;
	u8 gpio_reg_def;
	u32 hwen_gpio;
	u32 strobe_gpio;
	void (*camera_i2c_gpio_on) (void);
} __packed;

#endif	/* __KERNEL__ */

#endif	/* _LINUX_LED_FLASH_LM3554_H__ */
