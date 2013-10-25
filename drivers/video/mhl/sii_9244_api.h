/*
 * SiIxxxx <Firmware or Driver>
 *
 * Copyright (C) 2011 Silicon Image Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/types.h>

#ifndef __SII_9244_API_H__
#define __SII_9244_API_H__

typedef char Bool;
typedef	bool	bool_t;

#define LOW                     0
#define HIGH                    1

#define _ZERO		     0x00
#define BIT0                   0x01
#define BIT1                   0x02
#define BIT2                   0x04
#define BIT3                   0x08
#define BIT4                   0x10
#define BIT5                   0x20
#define BIT6                   0x40
#define BIT7                   0x80

#define MONITORING_PERIOD		50

extern bool	vbusPowerState;
extern uint16_t Int_count;

void		HalTimerWait(uint16_t m_sec);

extern	void	AppMhlTxDisableInterrupts(void);

extern	void	AppMhlTxRestoreInterrupts(void);

extern	void	AppVbusControl(bool powerOn);

#define	VBUS_POWER_CHK	ENABLE

#define	POWER_STATE_D3				3
#define	POWER_STATE_D0_NO_MHL		2
#define	POWER_STATE_D0_MHL			0
#define	POWER_STATE_FIRST_INIT		0xFF

#endif  /* __SII_9244_API_H__ */
