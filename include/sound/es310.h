#ifndef __LINUX_es310_H
#define __LINUX_es310_H
/*-----------------API ---------------------*/

#define A200_msg_BootloadInitiate 0x80030000

#define ES310_msg_BOOT		0x0001
#define ES310_msg_BOOT_ACK	0x01

#define A200_msg_Sync_Polling 0x80000000
#define SYNC_WAIT_TIME_BEFORE 10
#define A200_msg_Reset 0x8002
#define RESET_IMMEDIATE 0x0000
#define RESET_DELAYED 0x0001

#define A200_msg_Wakeup
#define WAKEUP_WAITTIME_AFTER 30

#define A200_msg_GetDeviceParm		0x800B
#define A200_msg_SetDeviceParmID	0x800C
#define A200_msg_SetDeviceParm		0x800D

#define A200_msg_SetPowerState_Sleep 0x80100001
#define STOP_CLOCK_WAITTIME_AFTER 120

#define ES310_I2C_NAME "audience_es310"
#define ES310_I2S_SLAVE_ADDRESS (0x3E)
#define POLLING_TIMEOUT 20
#define RESET_TIMEOUT 50

#define ES310_MAX_FW_SIZE	(32*4096)

#define TIMEOUT			20 /* ms */
#define RETRY_CNT		5
#define POLLING_RETRY_CNT	10
#define ES310_ERROR_CODE	0xffff
#define ES310_SLEEP		0
#define ES310_ACTIVE		1
#define ES310_CMD_FIFO_DEPTH	32 /* 128 / 4 = 32 */
#define ERROR			0xffffffff

/* ---------------------Stucture -------------------*/

struct es310_platform_data {
	uint32_t gpio_es310_reset;
	uint32_t gpio_es310_clk;
	uint32_t gpio_es310_wakeup;
	uint32_t gpio_es310_mic_switch;
	int (*power_setup)(int on);
};

struct ES310_config_data {
	unsigned int data_len;
	unsigned int mode_num;
	unsigned char *cmd_data;  /* [mode][cmd_len][cmds..] */
};

enum ES310_config_mode {
	ES310_CONFIG_FULL,
	ES310_CONFIG_VP
};

#define PRESET_BASE 0x80310000
#define ES310_PRESET_HANDSET_INCALL_NB		(PRESET_BASE)
#define ES310_PRESET_HEADSET_INCALL_NB		(PRESET_BASE + 1)
#define ES310_PRESET_HANDSFREE_REC_NB		(PRESET_BASE + 2)
#define ES310_PRESET_HANDSFREE_INCALL_NB	(PRESET_BASE + 3)
#define ES310_PRESET_HANDSET_INCALL_WB		(PRESET_BASE + 4)
#define ES310_PRESET_HEADSET_INCALL_WB		(PRESET_BASE + 5)
#define ES310_PRESET_AUDIOPATH_DISABLE		(PRESET_BASE + 6)
#define ES310_PRESET_HANDSFREE_INCALL_WB	(PRESET_BASE + 7)
#define ES310_PRESET_HANDSET_VOIP_WB		(PRESET_BASE + 8)
#define ES310_PRESET_HEADSET_VOIP_WB		(PRESET_BASE + 9)
#define ES310_PRESET_HANDSFREE_REC_WB		(PRESET_BASE + 10)
#define ES310_PRESET_HANDSFREE_VOIP_WB		(PRESET_BASE + 11)
#define ES310_PRESET_VOICE_RECOGNIZTION_WB	(PRESET_BASE + 12)
#define ES310_PRESET_HEADSET_REC_WB		(PRESET_BASE + 13)

enum ES310_PathID {
	ES310_PATH_SUSPEND = 0,
	ES310_PATH_HANDSET,
	ES310_PATH_HEADSET,
	ES310_PATH_HANDSFREE,
	ES310_PATH_BACKMIC,
	ES310_PATH_MAX
};

enum MIC_SWITCH_CONF {
	MIC_SWITCH_UNUSED,
	MIC_SWITCH_AUXILIARY_MIC,
	MIC_SWITCH_HEADSET_MIC,
};

enum ES310_NS_states {
	ES310_NS_STATE_AUTO,
	ES310_NS_STATE_OFF,
	ES310_NS_STATE_CT,
	ES310_NS_STATE_FT,
	ES310_NS_NUM_STATES
};

struct es310img {
	unsigned char *buf;
	unsigned img_size;
};

int es310_set_config(int newid, int mode);
int es310_sleep(void);

#define ES310_IOCTL_MAGIC ';'

#define ES310_BOOTUP_INIT _IOW(ES310_IOCTL_MAGIC, 1, struct es310img *)
#define ES310_SET_CONFIG _IOW(ES310_IOCTL_MAGIC, 2, unsigned int *)
#define ES310_SET_PARAM _IOW(ES310_IOCTL_MAGIC, 4, struct ES310_config_data *)
#define ES310_SYNC_CMD _IO(ES310_IOCTL_MAGIC, 9)
#define ES310_SLEEP_CMD _IO(ES310_IOCTL_MAGIC, 11)
#define ES310_RESET_CMD _IO(ES310_IOCTL_MAGIC, 12)
#define ES310_WAKEUP_CMD _IO(ES310_IOCTL_MAGIC, 13)
#define ES310_MDELAY _IOW(ES310_IOCTL_MAGIC, 14, unsigned int)
#define ES310_READ_FAIL_COUNT _IOR(ES310_IOCTL_MAGIC, 15, unsigned int *)
#define ES310_READ_SYNC_DONE _IOR(ES310_IOCTL_MAGIC, 16, bool *)
#define ES310_READ_DATA _IOR(ES310_IOCTL_MAGIC, 17, unsigned long *)
#define ES310_WRITE_MSG _IOW(ES310_IOCTL_MAGIC, 18, unsigned long)
#define ES310_SET_PRESET _IOW(ES310_IOCTL_MAGIC, 19, unsigned long)
#endif
