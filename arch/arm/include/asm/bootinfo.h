#ifndef ASM_BOOTINFO_H
#define ASM_BOOTINFO_H

typedef enum {
	PU_REASON_EVENT_KEYPAD,
	PU_REASON_EVENT_RTC,
	PU_REASON_EVENT_CABLE,
	PU_REASON_EVENT_SMPL,
	PU_REASON_EVENT_WDOG,
	PU_REASON_EVENT_USB_CHG,
	PU_REASON_EVENT_WALL_CHG,
	PU_REASON_EVENT_UNKNOWN,
	PU_REASON_EVENT_HWRST,
	PU_REASON_MAX
} powerup_reason_t;

enum {
	RS_REASON_EVENT_WDOG,
	RS_REASON_EVENT_MPM,
	RS_REASON_EVENT_SECRST,
	RS_REASON_EVENT_KPANIC,
	RS_REASON_EVENT_NORMAL,
	RS_REASON_EVENT_OTHER,
	RS_REASON_MAX
};

#define PWR_ON_EVENT_KEYPAD     0x1
#define PWR_ON_EVENT_RTC        0x2
#define PWR_ON_EVENT_CABLE      0x4
#define PWR_ON_EVENT_SMPL       0x8
#define PWR_ON_EVENT_WDOG       0x10
#define PWR_ON_EVENT_USB_CHG    0x20
#define PWR_ON_EVENT_WALL_CHG   0x40

#define RESTART_EVENT_WDOG		0x10000
#define RESTART_EVENT_MPM		0x20000
#define RESTART_EVENT_SRST		0x30000
#define RESTART_EVENT_SECRST		0x40000
#define RESTART_EVENT_KPANIC		0x80000
#define RESTART_EVENT_NORMAL		0x100000
#define RESTART_EVENT_OTHER		0x200000

unsigned int get_powerup_reason(void);
int is_abnormal_powerup(void);
void set_powerup_reason(unsigned int powerup_reason);
unsigned int get_hw_version(void);
void set_hw_version(unsigned int powerup_reason);
#endif /* ASM_BOOTINFO_H */
