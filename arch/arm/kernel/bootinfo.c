/*
 * bootinfo.c
 *
 * Copyright (C) 2011 Xiaomi Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/sysdev.h>
#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <linux/bitops.h>

#define MAX_PU_REASON_STR_LEN 64
static const char * const powerup_reasons[PU_REASON_MAX] = {
	[PU_REASON_EVENT_KEYPAD]	= "keypad",
	[PU_REASON_EVENT_RTC]		= "rtc",
	[PU_REASON_EVENT_CABLE]		= "cable",
	[PU_REASON_EVENT_SMPL]		= "smpl",
	[PU_REASON_EVENT_WDOG]		= "wdog",
	[PU_REASON_EVENT_USB_CHG]	= "usb_chg",
	[PU_REASON_EVENT_WALL_CHG]	= "wall_chg",
	[PU_REASON_EVENT_UNKNOWN]	= "unknown",
	[PU_REASON_EVENT_HWRST]		= "hw_reset",
};

static const char * const reset_reasons[RS_REASON_MAX] = {
	[RS_REASON_EVENT_WDOG]		= "wdog",
	[RS_REASON_EVENT_MPM]		= "mpm pu reset",
	[RS_REASON_EVENT_SECRST]	= "security control power on reset",
	[RS_REASON_EVENT_KPANIC]	= "kpanic",
	[RS_REASON_EVENT_NORMAL]	= "reboot",
	[RS_REASON_EVENT_OTHER]		= "other",
};

static struct kobject *bootinfo_kobj;
static powerup_reason_t powerup_reason;
static unsigned int hw_version;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define bootinfo_func_init(type, name, initval)	\
	static type name = (initval);	\
	type get_##name(void)		\
	{				\
		return name;		\
	}				\
	void set_##name(type __##name)	\
	{				\
		name = __##name;	\
	}				\
	EXPORT_SYMBOL(set_##name);	\
	EXPORT_SYMBOL(get_##name);

int is_abnormal_powerup(void)
{
	u32 pu_reason = get_powerup_reason();
	return pu_reason & (RESTART_EVENT_KPANIC | RESTART_EVENT_WDOG | RESTART_EVENT_OTHER);
}

static ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	u32 pu_reason;
	int pu_reason_index = PU_REASON_MAX;
	u32 reset_reason;
	int reset_reason_index = RS_REASON_MAX;

	pu_reason = get_powerup_reason();
	pu_reason_index = find_first_bit((unsigned long *)&pu_reason,
		sizeof(pu_reason)*BITS_PER_BYTE);
	if (pu_reason_index < PU_REASON_MAX && pu_reason_index >= 0  && pu_reason_index != 4) {
		s += snprintf(s, MAX_PU_REASON_STR_LEN, "%s", powerup_reasons[pu_reason_index]);
		printk(KERN_DEBUG "%s: pu_reason [0x%x], first non-zero bit" \
			" %d\n", __func__, pu_reason, pu_reason_index);
		goto out;
	}

	WARN(pu_reason_index != 4, "powerup reason is invalid value %x\n", pu_reason);
	reset_reason = pu_reason >> 16;
	reset_reason_index = find_first_bit((unsigned long *)&reset_reason,
		sizeof(reset_reason)*BITS_PER_BYTE);
	if (reset_reason_index < RS_REASON_MAX && reset_reason_index >= 0) {
		s += snprintf(s, MAX_PU_REASON_STR_LEN, "%s", reset_reasons[reset_reason_index]);
		printk(KERN_DEBUG "%s: rs_reason [0x%x], first non-zero bit" \
			" %d\n", __func__, reset_reason, reset_reason_index);
		goto out;
	};

	s += snprintf(s, MAX_PU_REASON_STR_LEN, "unknown reboot");
out:
	return s - buf;
}

static ssize_t powerup_reason_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return n;
}

bootinfo_attr(powerup_reason);
bootinfo_func_init(u32, powerup_reason, 0);
bootinfo_func_init(u32, hw_version, 0);

static struct attribute *g[] = {
	&powerup_reason_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (bootinfo_kobj == NULL) {
		printk("bootinfo_init: subsystem_register failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		printk(KERN_ERR "bootinfo_init: subsystem_register failed\n");
		goto sys_fail;
	}

	return ret;

sys_fail:
	kobject_del(bootinfo_kobj);
fail:
	return ret;

}

static void __exit bootinfo_exit(void)
{
	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);

