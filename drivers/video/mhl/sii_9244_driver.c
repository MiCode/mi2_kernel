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

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>

#include "sii_9244_driver.h"
#include "sii_9244_api.h"

extern void	HalTimerWait(uint16_t m_sec);
extern uint8_t I2C_ReadByte(uint8_t SlaveAddr, uint8_t RegAddr);
extern void I2C_WriteByte(uint8_t SlaveAddr, uint8_t RegAddr, uint8_t Data);

mhlTx_config_t	mhlTxConfig = {0};

#define SILICON_IMAGE_ADOPTER_ID 322
#define TRANSCODER_DEVICE_ID 0x9244
#define	POWER_STATE_D3				3
#define	POWER_STATE_D0_NO_MHL		2
#define	POWER_STATE_D0_MHL			0
#define	POWER_STATE_FIRST_INIT		0xFF

static	uint8_t	fwPowerState = POWER_STATE_FIRST_INIT;

static	bool_t	deglitchingRsenNow;

static	bool_t	mscCmdInProgress;
static	uint8_t	dsHpdStatus;

uint8_t ReadBytePage0(uint8_t Offset)
{
	return I2C_ReadByte(PAGE_0_0X72, Offset);
}

void WriteBytePage0(uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_0_0X72, Offset, Data);
}

void ReadModifyWritePage0(uint8_t Offset, uint8_t Mask, uint8_t Data)
{
	uint8_t Temp;

	Temp = ReadBytePage0(Offset);
	Temp &= ~Mask;
	Temp |= (Data & Mask);
	WriteBytePage0(Offset, Temp);
}

uint8_t ReadByteCBUS(uint8_t Offset)
{
	return I2C_ReadByte(PAGE_CBUS_0XC8, Offset);
}

void WriteByteCBUS(uint8_t Offset, uint8_t Data)
{
	I2C_WriteByte(PAGE_CBUS_0XC8, Offset, Data);
}

void ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value)
{
	uint8_t Temp;

	Temp = ReadByteCBUS(Offset);
	Temp &= ~Mask;
	Temp |= (Value & Mask);
	WriteByteCBUS(Offset, Temp);
}


#define	I2C_READ_MODIFY_WRITE(saddr, offset, mask)	I1C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) | (mask));
#define ReadModifyWriteByteCBUS(offset, andMask, orMask)  WriteByteCBUS(offset, (ReadByteCBUS(offset)&andMask) | orMask)

#define	SET_BIT(saddr, offset, bitnumber)		I2C_READ_MODIFY_WRITE(saddr, offset, (1 << bitnumber))
#define	CLR_BIT(saddr, offset, bitnumber)		I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) & ~(1 << bitnumber))
#define	DISABLE_DISCOVERY				CLR_BIT(PAGE_0_0X72, 0x90, 0);
#define	ENABLE_DISCOVERY				SET_BIT(PAGE_0_0X72, 0x90, 0);

#define STROBE_POWER_ON                    	CLR_BIT(PAGE_0_0X72, 0x90, 1);
#define	INTR_4_DESIRED_MASK				(BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_INTR_4_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x78, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x78, 0x00)

#define	INTR_2_DESIRED_MASK				(BIT1)
#define	UNMASK_INTR_2_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x76, INTR_2_DESIRED_MASK)
#define	MASK_INTR_2_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x76, 0x00)


#define	INTR_1_DESIRED_MASK				(BIT5 | BIT6)
#define	UNMASK_INTR_1_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x75, INTR_1_DESIRED_MASK)
#define	MASK_INTR_1_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x75, 0x00)

#define	INTR_CBUS1_DESIRED_MASK			(BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_CBUS1_INTERRUPTS			WriteByteCBUS(0x09, INTR_CBUS1_DESIRED_MASK)
#define	MASK_CBUS1_INTERRUPTS			WriteByteCBUS(0x09, 0x00)

#define	INTR_CBUS2_DESIRED_MASK			(BIT0 | BIT2 | BIT3)
#define	UNMASK_CBUS2_INTERRUPTS			WriteByteCBUS(0x1F, INTR_CBUS2_DESIRED_MASK)
#define	MASK_CBUS2_INTERRUPTS			WriteByteCBUS(0x1F, 0x00)

#define	INTR_MHL_INT_0_DESIRED_MASK	(BIT0 | BIT1 | BIT2 | BIT3)
#define	UNMASK_MHL_INT_0_INTERRUPTS	WriteByteCBUS(0xF0, INTR_MHL_INT_0_DESIRED_MASK)
#define	MASK_MHL_INT_0_INTERRUPTS	WriteByteCBUS(0xF0, 0x00)

#define	INTR_MHL_INT_1_DESIRED_MASK	(BIT1)
#define	UNMASK_MHL_INT_1_INTERRUPTS	WriteByteCBUS(0xF1, INTR_MHL_INT_1_DESIRED_MASK)
#define	MASK_MHL_INT_1_INTERRUPTS	WriteByteCBUS(0xF1, 0x00)


#define	INTR_MHL_STATUS_0_DESIRED_MASK	(BIT0)
#define	UNMASK_MHL_STATUS_0_INTERRUPTS	WriteByteCBUS(0xE0, INTR_MHL_STATUS_0_DESIRED_MASK)
#define	MASK_MHL_STATUS_0_INTERRUPTS	WriteByteCBUS(0xE0, 0x00)

#define	INTR_MHL_STATUS_1_DESIRED_MASK	(BIT3)
#define	UNMASK_MHL_STATUS_1_INTERRUPTS	WriteByteCBUS(0xE1, INTR_MHL_STATUS_1_DESIRED_MASK)
#define	MASK_MHL_STATUS_1_INTERRUPTS	WriteByteCBUS(0xE1, 0x00)

#define	APPLY_PLL_RECOVERY

static void CbusReset(void)
{

	uint8_t	idx;
	SET_BIT(PAGE_0_0X72, 0x05, 3);
	HalTimerWait(2);
	CLR_BIT(PAGE_0_0X72, 0x05, 3);

	mscCmdInProgress = false;

	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;

	for (idx = 0; idx < 4; idx++) {
		WriteByteCBUS(0xE0 + idx, 0xFF);

		WriteByteCBUS(0xF0 + idx, 0xFF);
	}
}

static void InitCBusRegs(void)
{
	uint8_t	regval;

	TX_DEBUG_PRINT(("[MHL]: InitCBusRegs\n"));
	WriteByteCBUS(0x07, 0x32);
	WriteByteCBUS(0x40, 0x03);
	WriteByteCBUS(0x42, 0x06);
	WriteByteCBUS(0x36, 0x0C);

	WriteByteCBUS(0x3D, 0xFD);
	WriteByteCBUS(0x1C, 0x01);
	WriteByteCBUS(0x1D, 0x0F);

	WriteByteCBUS(0x44, 0x02);

	WriteByteCBUS(0x80, MHL_DEV_ACTIVE);
	WriteByteCBUS(0x81, MHL_VERSION);
	WriteByteCBUS(0x82, (MHL_DEV_CAT_SOURCE));
	WriteByteCBUS(0x83, (uint8_t)(SILICON_IMAGE_ADOPTER_ID >>   8));
	WriteByteCBUS(0x84, (uint8_t)(SILICON_IMAGE_ADOPTER_ID & 0xFF));
	WriteByteCBUS(0x85, MHL_DEV_VID_LINK_SUPPRGB444);
	WriteByteCBUS(0x86, MHL_DEV_AUD_LINK_2CH);
	WriteByteCBUS(0x87, 0);
	WriteByteCBUS(0x88, MHL_LOGICAL_DEVICE_MAP);
	WriteByteCBUS(0x89, 0);
	WriteByteCBUS(0x8A, (MHL_FEATURE_RCP_SUPPORT | MHL_FEATURE_RAP_SUPPORT | MHL_FEATURE_SP_SUPPORT));
	WriteByteCBUS(0x8B, (uint8_t)(TRANSCODER_DEVICE_ID >>   8));
	WriteByteCBUS(0x8C, (uint8_t)(TRANSCODER_DEVICE_ID & 0xFF));
	WriteByteCBUS(0x8D, MHL_SCRATCHPAD_SIZE);
	WriteByteCBUS(0x8E, MHL_INT_AND_STATUS_SIZE);
	WriteByteCBUS(0x8F, 0);

	regval = ReadByteCBUS(REG_CBUS_LINK_CONTROL_2);
	regval = (regval | 0x0C);
	WriteByteCBUS(REG_CBUS_LINK_CONTROL_2, regval);

	regval = ReadByteCBUS(REG_MSC_TIMEOUT_LIMIT);
	WriteByteCBUS(REG_MSC_TIMEOUT_LIMIT, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));

	WriteByteCBUS(REG_CBUS_LINK_CONTROL_1, 0x01);

	ReadModifyWriteCBUS(0x2E, BIT4 | BIT2 | BIT0, BIT4 | BIT2 | BIT0);
}

static void SiiMhlTxDrvAcquireUpstreamHPDControl(void)
{
	SET_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD Acquired.\n"));
}

static void SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow(void)
{
	ReadModifyWritePage0(0x79, BIT5 | BIT4, BIT4);
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD Acquired - driven low.\n"));
}

static void SiiMhlTxDrvReleaseUpstreamHPDControl(void)
{
	CLR_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD released.\n"));
}

static void WriteInitialRegisterValues(void)
{
	TX_DEBUG_PRINT(("[MHL]: WriteInitialRegisterValues\n"));
	I2C_WriteByte(PAGE_1_0X7A, 0x3D, 0x3F);
	I2C_WriteByte(PAGE_2_0X92, 0x11, 0x01);
	I2C_WriteByte(PAGE_2_0X92, 0x12, 0x15);
	I2C_WriteByte(PAGE_0_0X72, 0x08, 0x35);

	CbusReset();

	I2C_WriteByte(PAGE_2_0X92, 0x10, 0xC1);
	I2C_WriteByte(PAGE_2_0X92, 0x17, 0x03);
	I2C_WriteByte(PAGE_2_0X92, 0x1A, 0x20);
	I2C_WriteByte(PAGE_2_0X92, 0x22, 0x8A);
	I2C_WriteByte(PAGE_2_0X92, 0x23, 0x6A);
	I2C_WriteByte(PAGE_2_0X92, 0x24, 0xAA);
	I2C_WriteByte(PAGE_2_0X92, 0x25, 0xCA);
	I2C_WriteByte(PAGE_2_0X92, 0x26, 0xEA);
	I2C_WriteByte(PAGE_2_0X92, 0x4C, 0xA0);
	I2C_WriteByte(PAGE_2_0X92, 0x4D, 0x00);

	I2C_WriteByte(PAGE_0_0X72, 0x80, 0x34);
	I2C_WriteByte(PAGE_2_0X92, 0x45, 0x44);
	I2C_WriteByte(PAGE_2_0X92, 0x31, 0x0A);
	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);
	I2C_WriteByte(PAGE_0_0X72, 0xA1, 0xFC);

	I2C_WriteByte(PAGE_0_0X72, 0xA3, 0xEB);
	I2C_WriteByte(PAGE_0_0X72, 0xA6, 0x0C);

	I2C_WriteByte(PAGE_0_0X72, 0x2B, 0x01);

	ReadModifyWritePage0(0x90, BIT3 | BIT2, BIT2);

	I2C_WriteByte(PAGE_0_0X72, 0x91, 0xA5);

	I2C_WriteByte(PAGE_0_0X72, 0x94, 0x77);

	WriteByteCBUS(0x31, ReadByteCBUS(0x31) | 0x0c);

	I2C_WriteByte(PAGE_0_0X72, 0xA5, 0xA0);

	TX_DEBUG_PRINT(("[MHL]: MHL 1.0 Compliant Clock\n"));

#if !defined(CI2CA)
	I2C_WriteByte(PAGE_0_0X72, 0x95, 0x71);
#else
	I2C_WriteByte(PAGE_0_0X72, 0x95, 0x75);
	ReadModifyWritePage0(0x91, BIT3, BIT3);
	ReadModifyWritePage0(0x96, BIT5, 0x00);
#endif


	I2C_WriteByte(PAGE_0_0X72, 0x97, 0x00);


	I2C_WriteByte(PAGE_0_0X72, 0x92, 0x86);
	I2C_WriteByte(PAGE_0_0X72, 0x93, 0x8C);

	ReadModifyWritePage0(0x79, BIT6, 0x00);

	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

	HalTimerWait(25);
	ReadModifyWritePage0(0x95, BIT6, 0x00);

	I2C_WriteByte(PAGE_0_0X72, 0x90, 0x27);

	InitCBusRegs();

	I2C_WriteByte(PAGE_0_0X72, 0x05, 0x04);

	I2C_WriteByte(PAGE_0_0X72, 0x0D, 0x1C);

	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;
	UNMASK_MHL_INT_0_INTERRUPTS;
	UNMASK_MHL_INT_1_INTERRUPTS;
	UNMASK_MHL_STATUS_0_INTERRUPTS;
	UNMASK_MHL_STATUS_1_INTERRUPTS;
}

static void ForceUsbIdSwitchOpen(void)
{
	DISABLE_DISCOVERY
		ReadModifyWritePage0(0x95, BIT6, BIT6);

	WriteBytePage0(0x92, 0x86);

	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();
}

static void ReleaseUsbIdSwitchOpen(void)
{
	HalTimerWait(50);

	ReadModifyWritePage0(0x95, BIT6, 0x00);

	ENABLE_DISCOVERY;
}

static void	SwitchToD0(void)
{
	TX_DEBUG_PRINT(("[MHL]: [%d]: Switch To Full power mode (D0)\n",
				(int) (HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));

	WriteInitialRegisterValues();

	STROBE_POWER_ON

		fwPowerState = POWER_STATE_D0_NO_MHL;
}

static void SwitchToD3(void)
{
	{
		TX_DEBUG_PRINT(("[MHL]: Switch To D3\n"));

		ForceUsbIdSwitchOpen();

		ReadModifyWritePage0(0x93, BIT7 | BIT6 | BIT5 | BIT4, 0);

		ReadModifyWritePage0(0x94, BIT1 | BIT0, 0);

		ReleaseUsbIdSwitchOpen();

		SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

		I2C_WriteByte(PAGE_2_0X92, 0x01, 0x03);

		CLR_BIT(PAGE_1_0X7A, 0x3D, 0);

		fwPowerState = POWER_STATE_D3;
	}

#if (VBUS_POWER_CHK == ENABLE)
	if (vbusPowerState == false)
		AppVbusControl(vbusPowerState = true);
#endif
}

static void MhlTxDrvProcessConnection(void)
{
	bool_t	mhlConnected = true;

	if (0x02 != (I2C_ReadByte(PAGE_0_0X72, 0x99) & 0x03)) {
		TX_DEBUG_PRINT (("[MHL]: MHL_EST interrupt but not MHL impedance\n"));
		SwitchToD0();
		SwitchToD3();
		return;
	}

	TX_DEBUG_PRINT (("[MHL]: MHL Cable Connected. CBUS:0x0A = %02X\n",
		(int)ReadByteCBUS(0x0a)));

	if (POWER_STATE_D0_MHL == fwPowerState)
		return;

	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0x10);

	fwPowerState = POWER_STATE_D0_MHL;

	WriteByteCBUS(0x07, 0x32);

	SET_BIT(PAGE_CBUS_0XC8, 0x44, 1);

	I2C_WriteByte(PAGE_2_0X92, 0x01, 0x00);

	ENABLE_DISCOVERY;

	TX_DEBUG_PRINT (("[MHL]: [%d]: Wait T_SRC_RXSENSE_CHK (%d ms) before checking RSEN\n",
				(int) (HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD),
				(int) T_SRC_RXSENSE_CHK));

	HalTimerSet(TIMER_TO_DO_RSEN_CHK, T_SRC_RXSENSE_CHK);
	UNMASK_INTR_1_INTERRUPTS;
	SiiMhlTxNotifyConnection(mhlConnected = true);
}

static void MhlTxDrvProcessDisconnection(void)
{
	bool_t	mhlConnected = false;

	TX_DEBUG_PRINT (("[MHL]: [%d]: MhlTxDrvProcessDisconnection\n", (int) (HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));

	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);

	SiiMhlTxDrvTmdsControl(false);

	if (POWER_STATE_D0_MHL == fwPowerState)
		SiiMhlTxNotifyConnection(mhlConnected = false);

	SwitchToD3();
}

static void CbusWakeUpPulseGenerator(void)
{
#ifdef CbusWakeUpPulse_GPIO
	TX_DEBUG_PRINT(("[MHL]: CbusWakeUpPulseGenerator: GPIO mode\n"));
	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0x08));

	pinMHLTxCbusWakeUpPulse = 1;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 0;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 1;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 0;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_2);

	pinMHLTxCbusWakeUpPulse = 1;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 0;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 1;
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	pinMHLTxCbusWakeUpPulse = 0;
	HalTimerWait(T_SRC_WAKE_TO_DISCOVER);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0xF7));

	I2C_WriteByte(PAGE_0_0X72, 0x90, (I2C_ReadByte(PAGE_0_0X72, 0x90) & 0xFE));
	I2C_WriteByte(PAGE_0_0X72, 0x90, (I2C_ReadByte(PAGE_0_0X72, 0x90) | 0x01));

#else
	TX_DEBUG_PRINT(("[MHL]: CbusWakeUpPulseGenerator: I2C mode\n"));
	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_2);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));

	HalTimerWait(T_SRC_WAKE_TO_DISCOVER);
#endif
}

static void	ProcessRgnd(void)
{
	uint8_t		reg99RGNDRange;
	reg99RGNDRange = I2C_ReadByte(PAGE_0_0X72, 0x99) & 0x03;
	TX_DEBUG_PRINT(("[MHL]: RGND Reg 99 = %02X\n", (int)reg99RGNDRange));

	if (0x02 == reg99RGNDRange) {
		SwitchToD0();

		SET_BIT(PAGE_0_0X72, 0x95, 5);

#if (VBUS_POWER_CHK == ENABLE)
		AppVbusControl(vbusPowerState = false);
#endif

		TX_DEBUG_PRINT(("[MHL]: Waiting T_SRC_VBUS_CBUS_TO_STABLE (%d ms)\n", (int)T_SRC_VBUS_CBUS_TO_STABLE));
		HalTimerWait(T_SRC_VBUS_CBUS_TO_STABLE);

		CbusWakeUpPulseGenerator();

		HalTimerSet(ELAPSED_TIMER1, T_SRC_DISCOVER_TO_MHL_EST);
	} else {
		TX_DEBUG_PRINT(("[MHL]: USB impedance. Set for USB Established.\n"));

		CLR_BIT(PAGE_0_0X72, 0x95, 5);
	}
}

static void Int4Isr(void)
{
	uint8_t reg74;

	reg74 = I2C_ReadByte(PAGE_0_0X72, (0x74));

	if (0xFF == reg74 || 0x87 == reg74)
		return;

	if (reg74 & BIT2) {
		HalTimerSet(ELAPSED_TIMER1, 0);
		MhlTxDrvProcessConnection();
	}

	else if (reg74 & BIT3)
		MhlTxDrvProcessDisconnection();

	if ((POWER_STATE_D3 == fwPowerState) && (reg74 & BIT6))
		ProcessRgnd();

	if (reg74 & BIT4) {
		TX_DEBUG_PRINT(("Drv: CBus Lockout\n"));

		SwitchToD0();
		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();
		SwitchToD3();
	}
	I2C_WriteByte(PAGE_0_0X72, (0x74), reg74);
}

static void DeglitchRsenLow(void)
{
	TX_DEBUG_PRINT(("[MHL]: DeglitchRsenLow RSEN <72:09[2]> = %02X\n", (int) (I2C_ReadByte(PAGE_0_0X72, 0x09))));

	if ((I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2) == 0x00) {
		TX_DEBUG_PRINT(("[MHL]: [%d]: RSEN is Low.\n", (int) (HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));
		if ((POWER_STATE_D0_MHL == fwPowerState) && HalTimerExpired(TIMER_TO_DO_RSEN_DEGLITCH)) {
			TX_DEBUG_PRINT(("[MHL]: Disconnection due to RSEN Low\n"));

			deglitchingRsenNow = false;

			DISABLE_DISCOVERY;
			ENABLE_DISCOVERY;
			dsHpdStatus &= ~BIT6;
			WriteByteCBUS(0x0D, dsHpdStatus);
			SiiMhlTxNotifyDsHpdChange(0);
			MhlTxDrvProcessDisconnection();
		}
	} else
		deglitchingRsenNow = false;
}

void	Int1RsenIsr(void)
{
	uint8_t	reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);
	uint8_t	rsen  = I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2;
	if ((reg71 & BIT5) ||
			((false == deglitchingRsenNow) && (rsen == 0x00))) {
		TX_DEBUG_PRINT(("[MHL]: Got INTR_1: reg71 = %02X, rsen = %02X\n", (int)reg71, (int)rsen));
		if (rsen == 0x00) {
			TX_DEBUG_PRINT(("[MHL]: Int1RsenIsr: Start T_SRC_RSEN_DEGLITCH (%d ms) before disconnection\n",
				(int)(T_SRC_RSEN_DEGLITCH)));
			HalTimerSet(TIMER_TO_DO_RSEN_DEGLITCH, T_SRC_RSEN_DEGLITCH);

			deglitchingRsenNow = true;

			while (deglitchingRsenNow == true && POWER_STATE_D0_MHL == fwPowerState) {
				HalTimerWait(10);
				TX_DEBUG_PRINT(("[MHL]: [%d]: deglitchingRsenNow.\n",
					(int)(HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));
				DeglitchRsenLow();
			}
		} else if (deglitchingRsenNow) {
			TX_DEBUG_PRINT(("[MHL]: [%d]: Ignore now, RSEN is high. This was a glitch.\n",
				(int)(HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));
			deglitchingRsenNow = false;
		}
		I2C_WriteByte(PAGE_0_0X72, 0x71, BIT5);
	} else if (deglitchingRsenNow) {
		TX_DEBUG_PRINT(("[MHL]: [%d]: Ignore now coz (reg71 & BIT5) has been cleared. This was a glitch.\n",
			(int)(HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));
		deglitchingRsenNow = false;
	}
}

static void ApplyDdcAbortSafety(void)
{
	uint8_t	bTemp, bPost;

	/*	TX_DEBUG_PRINT(("[MHL]: [%d]: Do we need DDC Abort Safety\n",
		(int) (HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));*/

	WriteByteCBUS(0x29, 0xFF);
	bTemp = ReadByteCBUS(0x29);
	HalTimerWait(3);
	bPost = ReadByteCBUS(0x29);

	TX_DEBUG_PRINT(("[MHL]: bTemp: 0x%X bPost: 0x%X\n", (int)bTemp, (int)bPost));

	if (bPost > (bTemp + 50)) {
		TX_DEBUG_PRINT(("[MHL]: Applying DDC Abort Safety(SWWA 18958)\n"));

		CbusReset();

		InitCBusRegs();

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();

		MhlTxDrvProcessDisconnection();
	}

}

static uint8_t CBusProcessErrors(uint8_t intStatus)
{
	uint8_t result          = 0;
	uint8_t mscAbortReason  = 0;
	uint8_t ddcAbortReason  = 0;

	/* At this point, we only need to look at the abort interrupts. */
	intStatus &=  (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT);

	if (intStatus) {
		/* If transfer abort or MSC abort, clear the abort reason register. */
		if (intStatus & BIT_DDC_ABORT) {
			result = ddcAbortReason = ReadByteCBUS(REG_DDC_ABORT_REASON);
			TX_DEBUG_PRINT(("[MHL]: CBUS:: DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason)));
		}

		if (intStatus & BIT_MSC_XFR_ABORT) {
			result = mscAbortReason = ReadByteCBUS(REG_PRI_XFR_ABORT_REASON);

			TX_DEBUG_PRINT(("[MHL]: CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n"));
			WriteByteCBUS(REG_PRI_XFR_ABORT_REASON, 0xFF);
		}
		if (intStatus & BIT_MSC_ABORT) {
			TX_DEBUG_PRINT(("[MHL]: CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n"));
			WriteByteCBUS(REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF);
		}

		if (mscAbortReason != 0) {
			TX_DEBUG_PRINT(("[MHL]: CBUS:: Reason for ABORT is ....0x%02X\n", (int)mscAbortReason));

			if (mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)
				TX_DEBUG_PRINT(("[MHL]: CBUS:: Requestor MAXFAIL - retry threshold exceeded\n"));
			if (mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)
				TX_DEBUG_PRINT(("[MHL]: CBUS:: Protocol Error\n"));
			if (mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)
				TX_DEBUG_PRINT(("[MHL]: CBUS:: Requestor translation layer timeout\n"));
			if (mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)
				TX_DEBUG_PRINT(("[MHL]: CBUS:: Peer sent an abort\n"));
			if (mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)
				TX_DEBUG_PRINT(("[MHL]: CBUS:: Undefined opcode\n"));
		}
	}
	return result;
}

static void MhlCbusIsr(void)
{
	uint8_t	cbusInt;
	uint8_t    gotData[4];
	uint8_t	i;
	uint8_t	reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);

	cbusInt = ReadByteCBUS(0x08);

	if (cbusInt == 0xFF)
		return;

	cbusInt &= (~(BIT1 | BIT0));
	if (cbusInt) {
		WriteByteCBUS(0x08, cbusInt);

		TX_DEBUG_PRINT(("[MHL]: Clear CBUS INTR_1: %02X\n", (int) cbusInt));
	}

	if (cbusInt & BIT2)
		ApplyDdcAbortSafety();
	if ((cbusInt & BIT3)) {
		uint8_t mscMsg[2];
		TX_DEBUG_PRINT(("[MHL]: MSC_MSG Received\n"));
		mscMsg[0] = ReadByteCBUS(0x18);
		mscMsg[1] = ReadByteCBUS(0x19);

		TX_DEBUG_PRINT(("[MHL]: MSC MSG: %02X %02X\n", (int)mscMsg[0], (int)mscMsg[1]));
		SiiMhlTxGotMhlMscMsg(mscMsg[0], mscMsg[1]);
	}
	if ((cbusInt & BIT5) || (cbusInt & BIT6)) {
		gotData[0] = CBusProcessErrors(cbusInt);
		mscCmdInProgress = false;
	}
	if (cbusInt & BIT4) {
		TX_DEBUG_PRINT(("[MHL]: MSC_REQ_DONE\n"));

		mscCmdInProgress = false;
		SiiMhlTxMscCommandDone(ReadByteCBUS(0x16));
	}

	if (BIT7 & cbusInt) {
#define CBUS_LINK_STATUS_2 0x38
		TX_DEBUG_PRINT(("[MHL]: Clearing CBUS_link_hard_err_count\n"));
		WriteByteCBUS(CBUS_LINK_STATUS_2, (uint8_t)(ReadByteCBUS(CBUS_LINK_STATUS_2) & 0xF0));
	}
	cbusInt = ReadByteCBUS(0x1E);
	if (cbusInt) {
		WriteByteCBUS(0x1E, cbusInt);

		TX_DEBUG_PRINT(("[MHL]: Clear CBUS INTR_2: %02X\n", (int)cbusInt));
	}
	if (BIT0 & cbusInt)
		SiiMhlTxMscWriteBurstDone(cbusInt);
	if (cbusInt & BIT2) {
		uint8_t intr[4];
		uint8_t address;

		TX_DEBUG_PRINT(("[MHL]: MHL INTR Received\n"));
		for(i = 0, address = 0xA0; i < 4; ++i, ++address) {
			intr[i] = ReadByteCBUS(address);
			WriteByteCBUS(address, intr[i]);
		}
		SiiMhlTxGotMhlIntr(intr[0], intr[1]);

	}
	if ((cbusInt & BIT3) || HalTimerExpired(TIMER_SWWA_WRITE_STAT)) {
		uint8_t status[4];
		uint8_t address;

		for (i = 0, address=0xB0; i < 4; ++i, ++address) {
			status[i] = ReadByteCBUS(address);
			WriteByteCBUS(address , 0xFF /* future status[i]*/);
		}
		SiiMhlTxGotMhlStatus(status[0], status[1]);
		HalTimerSet(TIMER_SWWA_WRITE_STAT, T_SWWA_WRITE_STAT);
	}

	if (reg71)
		I2C_WriteByte(PAGE_0_0X72, 0x71, reg71);
	cbusInt = ReadByteCBUS(0x0D);

	if (BIT6 & (dsHpdStatus ^ cbusInt)) {
		dsHpdStatus = cbusInt;

		TX_DEBUG_PRINT(("[MHL]: Downstream HPD changed to: %02X\n", (int) cbusInt));
		SiiMhlTxNotifyDsHpdChange(BIT6 & cbusInt);
	}
}

#ifdef	APPLY_PLL_RECOVERY
static void ApplyPllRecovery (void)
{
	CLR_BIT(PAGE_0_0X72, 0x80, 4);

	SET_BIT(PAGE_0_0X72, 0x80, 4);

	HalTimerWait(10);

	SET_BIT(PAGE_0_0X72, 0x05, 4);

	CLR_BIT(PAGE_0_0X72, 0x05, 4);

	TX_DEBUG_PRINT(("[MHL]: Applied PLL Recovery\n"));
}

static void SiiMhlTxDrvRecovery(void)
{
	if ((I2C_ReadByte(PAGE_0_0X72, (0x74)) & BIT0)) {
		TX_DEBUG_PRINT(("[MHL]: SCDT Interrupt\n"));
		SET_BIT(PAGE_0_0X72, (0x74), 0);

		if (((I2C_ReadByte(PAGE_0_0X72, 0x81)) & BIT1) >> 1)
			ApplyPllRecovery();
	}
	if ((I2C_ReadByte(PAGE_0_0X72, (0x72)) & BIT1)) {

		TX_DEBUG_PRINT(("[MHL]: PSTABLE Interrupt\n"));

		ApplyPllRecovery();

		SET_BIT(PAGE_0_0X72, (0x72), 1);

	}
}
#endif

bool_t SiiMhlTxChipInitialize(void)
{

	HalTimerSet(ELAPSED_TIMER, MONITORING_PERIOD);

	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxChipInitialize: 92%02X\n", (int)I2C_ReadByte(PAGE_0_0X72, 0x02)));

	WriteInitialRegisterValues();

	I2C_WriteByte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK);

	SwitchToD3();

	return true;
}

void SiiMhlTxDeviceIsr(void)
{
	if (POWER_STATE_D0_MHL != fwPowerState)

		Int4Isr();
	else if (POWER_STATE_D0_MHL == fwPowerState) {
		while ((I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2) == 0x00 &&
				(!HalTimerExpired(TIMER_TO_DO_RSEN_CHK))) {
			TX_DEBUG_PRINT(("[MHL]: g_timerCounters[TIMER_TO_DO_RSEN_CHK]=%d.\n",i
				g_timerCounters[TIMER_TO_DO_RSEN_CHK]));
			HalTimerWait(10);
		}
		if (HalTimerExpired(TIMER_TO_DO_RSEN_CHK)) {
			if (deglitchingRsenNow) {
				TX_DEBUG_PRINT(("[MHL]: [%d]: deglitchingRsenNow.\n",
					(int)(HalTimerElapsed(ELAPSED_TIMER) * MONITORING_PERIOD)));
				DeglitchRsenLow();
			} else
				Int1RsenIsr();
		}


		if (POWER_STATE_D0_MHL != fwPowerState)
			return;

#ifdef	APPLY_PLL_RECOVERY
		SiiMhlTxDrvRecovery();

#endif
		MhlCbusIsr();
	}

}

void	SiiMhlTxDrvTmdsControl(bool_t enable)
{
	if (enable) {
		SET_BIT(PAGE_0_0X72, 0x80, 4);
		TX_DEBUG_PRINT(("[MHL]: MHL Output Enabled\n"));
		SiiMhlTxDrvReleaseUpstreamHPDControl();
	} else {
		CLR_BIT(PAGE_0_0X72, 0x80, 4);
		TX_DEBUG_PRINT(("[MHL]: MHL Ouput Disabled\n"));
	}
}
void	SiiMhlTxDrvNotifyEdidChange(void)
{
	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxDrvNotifyEdidChange\n"));
	SiiMhlTxDrvAcquireUpstreamHPDControl();

	CLR_BIT(PAGE_0_0X72, 0x79, 5);

	HalTimerWait(110);

	SET_BIT(PAGE_0_0X72, 0x79, 5);
}

bool_t SiiMhlTxDrvSendCbusCommand(cbus_req_t *pReq)
{
	bool_t  success = true;

	uint8_t i, startbit;

	if ((POWER_STATE_D0_MHL != fwPowerState) || (mscCmdInProgress)) {
		TX_DEBUG_PRINT(("[MHL]: Error: fwPowerState: %02X, or CBUS(0x0A):%02X mscCmdInProgress = %d\n",
					(int) fwPowerState,
					(int) ReadByteCBUS(0x0a),
					(int) mscCmdInProgress));

		return false;
	}
	mscCmdInProgress = true;

	TX_DEBUG_PRINT(("[MHL]: Sending MSC command %02X, %02X, %02X, %02X\n",
				(int)pReq->command,
				(int)(pReq->offsetData),
				(int)pReq->payload_u.msgData[0],
				(int)pReq->payload_u.msgData[1]));

	/****************************************************************************************/
	/* Setup for the command - write appropriate registers and determine the correct        */
	/*                         start bit.                                                   */
	/****************************************************************************************/

	WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->offsetData);
	WriteByteCBUS((REG_CBUS_PRI_WR_DATA_1ST & 0xFF), pReq->payload_u.msgData[0]);

	startbit = 0x00;
	switch (pReq->command) {
	case MHL_SET_INT:
		startbit = MSC_START_BIT_WRITE_REG;
		break;

	case MHL_WRITE_STAT:
		startbit = MSC_START_BIT_WRITE_REG;
		break;

	case MHL_READ_DEVCAP:
		startbit = MSC_START_BIT_READ_REG;
		break;

	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command);
		startbit = MSC_START_BIT_MSC_CMD;
		break;

	case MHL_MSC_MSG:
		WriteByteCBUS((REG_CBUS_PRI_WR_DATA_2ND & 0xFF), pReq->payload_u.msgData[1]);
		WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command);
		startbit = MSC_START_BIT_VS_CMD;
		break;

	case MHL_WRITE_BURST:
		ReadModifyWriteCBUS((REG_MSC_WRITE_BURST_LEN & 0xFF), 0x0F, pReq->length - 1);

		if (NULL == pReq->payload_u.pdatabytes)
			TX_DEBUG_PRINT(("[MHL]: Put pointer to WRITE_BURST data in req.pdatabytes!!!\n\n"));
		else {
			uint8_t *pData = pReq->payload_u.pdatabytes;
			TX_DEBUG_PRINT(("[MHL]: Writing data into scratchpad\n\n"));
			for (i = 0; i < pReq->length; i++)
				WriteByteCBUS((REG_CBUS_SCRATCHPAD_0 & 0xFF) + i, *pData++);
		}
		startbit = MSC_START_BIT_WRITE_BURST;
		break;

	default:
		success = false;
		break;
	}

	/****************************************************************************************/
	/* Trigger the CBUS command transfer using the determined start bit.                    */
	/****************************************************************************************/

	if (success)
		WriteByteCBUS(REG_CBUS_PRI_START & 0xFF, startbit);
	else
		TX_DEBUG_PRINT(("[MHL]: SiiMhlTxDrvSendCbusCommand failed\n\n"));

	return success;
}

bool_t SiiMhlTxDrvCBusBusy(void)
{
	return mscCmdInProgress ? true : false;
}

void SiiMhlTxDrvGetScratchPad(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	int i;
	uint8_t regOffset;

	for (regOffset = 0xC0 + startReg, i = 0; i < length; ++i, ++regOffset)
		*pData++ = ReadByteCBUS(regOffset);
}

/*
   queue implementation
 */
#define NUM_CBUS_EVENT_QUEUE_EVENTS 5
typedef struct _CBusQueue_t {
	uint8_t head;
	uint8_t tail;
	cbus_req_t queue[NUM_CBUS_EVENT_QUEUE_EVENTS];
} CBusQueue_t, *PCBusQueue_t;

#define QUEUE_SIZE(x) (sizeof(x.queue)/sizeof(x.queue[0]))
#define MAX_QUEUE_DEPTH(x) (QUEUE_SIZE(x) -1)
#define QUEUE_DEPTH(x) ((x.head <= x.tail) ? (x.tail - x.head) : (QUEUE_SIZE(x) - x.head + x.tail))
#define QUEUE_FULL(x) (QUEUE_DEPTH(x) >= MAX_QUEUE_DEPTH(x))

#define ADVANCE_QUEUE_HEAD(x) { x.head = (x.head < MAX_QUEUE_DEPTH(x)) ? (x.head+1) : 0; }
#define ADVANCE_QUEUE_TAIL(x) { x.tail = (x.tail < MAX_QUEUE_DEPTH(x)) ? (x.tail+1) : 0; }

#define RETREAT_QUEUE_HEAD(x) { x.head = (x.head > 0) ? (x.head-1) : MAX_QUEUE_DEPTH(x); }


CBusQueue_t CBusQueue;

cbus_req_t *GetNextCBusTransactionImpl(void)
{
	if (0 == QUEUE_DEPTH(CBusQueue))
		return NULL;
	else {
		cbus_req_t *retVal;
		retVal = &CBusQueue.queue[CBusQueue.head];
		ADVANCE_QUEUE_HEAD(CBusQueue)
		return retVal;
	}
}

cbus_req_t *GetNextCBusTransactionWrapper(char *pszFunction, int iLine)
{
	TX_DEBUG_PRINT(("[MHL]:%d %s\n", iLine, pszFunction));
	return  GetNextCBusTransactionImpl();
}
#define GetNextCBusTransaction(func) GetNextCBusTransactionWrapper(#func, __LINE__)

bool_t PutNextCBusTransactionImpl(cbus_req_t *pReq)
{
	if (QUEUE_FULL(CBusQueue))
		return false;
	CBusQueue.queue[CBusQueue.tail] = *pReq;
	ADVANCE_QUEUE_TAIL(CBusQueue)
		return true;
}
bool_t PutNextCBusTransactionWrapper(cbus_req_t *pReq, int iLine)
{
	bool_t retVal;

	TX_DEBUG_PRINT(("[MHL]:%d PutNextCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n",
		iLine, (int)pReq->command,
		(int)((MHL_MSC_MSG == pReq->command) ? pReq->payload_u.msgData[0] : pReq->offsetData),
		(int)((MHL_MSC_MSG == pReq->command) ? pReq->payload_u.msgData[1] : pReq->payload_u.msgData[0]),
		(int)QUEUE_DEPTH(CBusQueue), (int)CBusQueue.head, (int)CBusQueue.tail));
	retVal = PutNextCBusTransactionImpl(pReq);

	if (!retVal)
		TX_DEBUG_PRINT(("[MHL]:%d PutNextCBusTransaction queue full, when adding event %d\n",
			iLine, (int)pReq->command));
	return retVal;
}
#define PutNextCBusTransaction(req) PutNextCBusTransactionWrapper(req, __LINE__)

bool_t PutPriorityCBusTransactionImpl(cbus_req_t *pReq)
{
	if (QUEUE_FULL(CBusQueue))
		return false;
	RETREAT_QUEUE_HEAD(CBusQueue)
		CBusQueue.queue[CBusQueue.head] = *pReq;
	return true;
}

bool_t PutPriorityCBusTransactionWrapper(cbus_req_t *pReq, int iLine)
{
	bool_t retVal;
	TX_DEBUG_PRINT(("[MHL]:%d: PutPriorityCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n",
		iLine, (int)pReq->command,
		(int)((MHL_MSC_MSG == pReq->command) ? pReq->payload_u.msgData[0] : pReq->offsetData),
		(int)((MHL_MSC_MSG == pReq->command) ? pReq->payload_u.msgData[1] : pReq->payload_u.msgData[0]),
		(int)QUEUE_DEPTH(CBusQueue),
		(int)CBusQueue.head,
		(int)CBusQueue.tail));
	retVal = PutPriorityCBusTransactionImpl(pReq);
	if (!retVal)
		TX_DEBUG_PRINT(("[MHL]:%d: PutPriorityCBusTransaction queue full, when adding event 0x%02X\n",
			iLine, (int)pReq->command));
	return retVal;
}
#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionWrapper(pReq, __LINE__)

#define IncrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount++; TX_DEBUG_PRINT(("[MHL]:%d %s cbusReferenceCount:%d\n", (int)__LINE__, #func, (int)mhlTxConfig.cbusReferenceCount)); }
#define DecrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount--; TX_DEBUG_PRINT(("[MHL]:%d %s cbusReferenceCount:%d\n", (int)__LINE__, #func, (int)mhlTxConfig.cbusReferenceCount)); }

#define SetMiscFlag(func, x) { mhlTxConfig.miscFlags |=  (x); TX_DEBUG_PRINT(("[MHL]:%d %s set %s\n", (int)__LINE__, #func, #x)); }
#define ClrMiscFlag(func, x) { mhlTxConfig.miscFlags &= ~(x); TX_DEBUG_PRINT(("[MHL]:%d %s clr %s\n", (int)__LINE__, #func, #x)); }

static bool_t SiiMhlTxSetDCapRdy(void);
static bool_t SiiMhlTxSetPathEn(void);
static bool_t SiiMhlTxClrPathEn(void);
static bool_t SiiMhlTxRapkSend(void);
static void		MhlTxDriveStates(void);
static void		MhlTxResetStates(void);
static bool_t	MhlTxSendMscMsg(uint8_t command, uint8_t cmdData);
extern uint8_t	rcpSupportTable[];

bool_t MhlTxCBusBusy(void)
{
	return ((QUEUE_DEPTH(CBusQueue) > 0) || SiiMhlTxDrvCBusBusy() || mhlTxConfig.cbusReferenceCount) ? true : false;
}

static void SiiMhlTxTmdsEnable(void)
{
	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxTmdsEnable\n"));
	if (MHL_RSEN & mhlTxConfig.mhlHpdRSENflags) {
		TX_DEBUG_PRINT(("\tMHL_RSEN\n"));
		if (MHL_HPD & mhlTxConfig.mhlHpdRSENflags) {
			TX_DEBUG_PRINT(("\t\tMHL_HPD\n"));
			if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.status_1) {
				TX_DEBUG_PRINT(("\t\t\tMHL_STATUS_PATH_ENABLED\n"));
				SiiMhlTxDrvTmdsControl(true);
			}
		}
	}
}

static bool_t SiiMhlTxSetInt(uint8_t regToWrite, uint8_t  mask, uint8_t priorityLevel)
{
	cbus_req_t	req;
	bool_t retVal;

	req.retryCount  = 2;
	req.command     = MHL_SET_INT;
	req.offsetData  = regToWrite;
	req.payload_u.msgData[0]  = mask;
	if (0 == priorityLevel)
		retVal = PutPriorityCBusTransaction(&req);
	else
		retVal = PutNextCBusTransaction(&req);
	return retVal;
}

static bool_t SiiMhlTxDoWriteBurst(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	if (FLAGS_WRITE_BURST_PENDING & mhlTxConfig.miscFlags) {
		cbus_req_t	req;
		bool_t retVal;

		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxDoWriteBurst startReg:%d length:%d\n",
			(int)__LINE__, (int)startReg, (int)length));

		req.retryCount  = 1;
		req.command     = MHL_WRITE_BURST;
		req.length      = length;
		req.offsetData  = startReg;
		req.payload_u.pdatabytes  = pData;

		retVal = PutPriorityCBusTransaction(&req);
		ClrMiscFlag(MhlTxDriveStates, FLAGS_WRITE_BURST_PENDING)
			return retVal;
	}
	return false;
}

bool_t SiiMhlTxRequestWriteBurst(void)
{
	bool_t retVal = false;

	if ((FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags) || MhlTxCBusBusy())

		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxRequestWriteBurst failed FLAGS_SCRATCHPAD_BUSY \n", (int)__LINE__));
	else {
		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxRequestWriteBurst, request sent\n", (int)__LINE__));
		retVal =  SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_REQ_WRT, 1);
	}

	return retVal;
}

void SiiMhlTxInitialize(bool_t interruptDriven, uint8_t pollIntervalMs)
{
	CBusQueue.head = 0;
	CBusQueue.tail = 0;

	mhlTxConfig.interruptDriven = interruptDriven;
	mhlTxConfig.pollIntervalMs  = pollIntervalMs;

	MhlTxResetStates();
	SiiMhlTxChipInitialize();
}


#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)

uint8_t rcpSupportTable[MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),
	(MHL_DEV_LD_GUI),
	(MHL_DEV_LD_GUI),
	(MHL_DEV_LD_GUI),
	(MHL_DEV_LD_GUI),
	0, 0, 0, 0,
	(MHL_DEV_LD_GUI),
	0, 0, 0,
	(MHL_DEV_LD_GUI),
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0, 0, 0,
	(MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_AUDIO),
	0,
	0,
	0,
	0,
	0,
	0, 0, 0, 0, 0, 0, 0,
	0,

	(MHL_DEV_LD_SPEAKER),
	(MHL_DEV_LD_SPEAKER),
	(MHL_DEV_LD_SPEAKER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),
	(MHL_DEV_LD_RECORD),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	(MHL_DEV_LD_MEDIA),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),
	0, 0, 0,
	0,
	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	(MHL_DEV_LD_RECORD),
	(MHL_DEV_LD_RECORD),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),

	(MHL_DEV_LD_SPEAKER),
	(MHL_DEV_LD_SPEAKER),
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void SiiMhlTxGetEvents(uint8_t *event, uint8_t *eventParameter)
{
	if (false == mhlTxConfig.interruptDriven)
		SiiMhlTxDeviceIsr();

	MhlTxDriveStates();

	*event = MHL_TX_EVENT_NONE;
	*eventParameter = 0;

	if (mhlTxConfig.mhlConnectionEvent) {
		TX_DEBUG_PRINT(("[MHL]: SiiMhlTxGetEvents mhlConnectionEvent\n"));

		mhlTxConfig.mhlConnectionEvent = false;

		*event = mhlTxConfig.mhlConnected;
		*eventParameter = mhlTxConfig.mscFeatureFlag;

		if (MHL_TX_EVENT_DISCONNECTION == mhlTxConfig.mhlConnected)
			MhlTxResetStates();
		else if (MHL_TX_EVENT_CONNECTION == mhlTxConfig.mhlConnected)
			SiiMhlTxSetDCapRdy();
	} else if (mhlTxConfig.mscMsgArrived) {
		TX_DEBUG_PRINT(("[MHL]: SiiMhlTxGetEvents MSC MSG <%02X, %02X>\n",
					(int) (mhlTxConfig.mscMsgSubCommand),
					(int) (mhlTxConfig.mscMsgData)));

		mhlTxConfig.mscMsgArrived = false;

		switch (mhlTxConfig.mscMsgSubCommand) {
		case	MHL_MSC_MSG_RAP:
			if (MHL_RAP_CONTENT_ON == mhlTxConfig.mscMsgData)
				SiiMhlTxTmdsEnable();
			else if (MHL_RAP_CONTENT_OFF == mhlTxConfig.mscMsgData)
				SiiMhlTxDrvTmdsControl(false);
			SiiMhlTxRapkSend();
			break;

		case	MHL_MSC_MSG_RCP:
			if (MHL_LOGICAL_DEVICE_MAP & rcpSupportTable[mhlTxConfig.mscMsgData & 0x7F]) {
				*event          = MHL_TX_EVENT_RCP_RECEIVED;
				*eventParameter = mhlTxConfig.mscMsgData;
			} else {
				mhlTxConfig.mscSaveRcpKeyCode = mhlTxConfig.mscMsgData;
				SiiMhlTxRcpeSend(RCPE_INEEFECTIVE_KEY_CODE);
			}
			break;

		case	MHL_MSC_MSG_RCPK:
			*event = MHL_TX_EVENT_RCPK_RECEIVED;
			*eventParameter = mhlTxConfig.mscMsgData;
			DecrementCBusReferenceCount(SiiMhlTxGetEvents)
				mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscMsgLastCommand = 0;

			TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxGetEvents RCPK\n", (int)__LINE__));
			break;

		case	MHL_MSC_MSG_RCPE:
			*event = MHL_TX_EVENT_RCPE_RECEIVED;
			*eventParameter = mhlTxConfig.mscMsgData;
			break;

		case	MHL_MSC_MSG_RAPK:
			DecrementCBusReferenceCount(SiiMhlTxGetEvents)
				mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscMsgLastCommand = 0;
			TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxGetEvents RAPK\n", (int)__LINE__));
			break;

		default:
			break;
		}
	}
}

static void MhlTxDriveStates(void)
{
	if ((POWER_STATE_D0_MHL != fwPowerState) && HalTimerElapsed(ELAPSED_TIMER1)) {
		TX_DEBUG_PRINT(("[MHL]: MHL Discovery timeout!\n"));
		HalTimerSet(ELAPSED_TIMER1, 0);
		MhlTxDrvProcessDisconnection();
	}

	if (QUEUE_DEPTH(CBusQueue) > 0) {
		if (!SiiMhlTxDrvCBusBusy()) {
			int reQueueRequest = 0;
			cbus_req_t *pReq = GetNextCBusTransaction(MhlTxDriveStates);
			if (MHL_SET_INT == pReq->command) {
				if (MHL_RCHANGE_INT == pReq->offsetData) {
					if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags) {
						if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0])
							reQueueRequest = 1;
						else if (MHL_INT_GRT_WRT == pReq->payload_u.msgData[0])
							reQueueRequest = 0;
					} else {
						if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0]) {
							IncrementCBusReferenceCount(MhlTxDriveStates)
								SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
								SetMiscFlag(MhlTxDriveStates, FLAGS_WRITE_BURST_PENDING)
						} else if (MHL_INT_GRT_WRT == pReq->payload_u.msgData[0])
							SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
					}
				}
			}
			if (reQueueRequest) {
				if (pReq->retryCount-- > 0)
					PutNextCBusTransaction(pReq);
			} else {
				if (MHL_MSC_MSG == pReq->command) {
					mhlTxConfig.mscMsgLastCommand = pReq->payload_u.msgData[0];
					mhlTxConfig.mscMsgLastData    = pReq->payload_u.msgData[1];
				} else {
					mhlTxConfig.mscLastOffset  = pReq->offsetData;
					mhlTxConfig.mscLastData    = pReq->payload_u.msgData[0];
				}
				mhlTxConfig.mscLastCommand = pReq->command;

				IncrementCBusReferenceCount(MhlTxDriveStates)
					SiiMhlTxDrvSendCbusCommand(pReq);
			}
		}
	}
}

#define FLAG_OR_NOT(x) (FLAGS_HAVE_##x & mhlTxConfig.miscFlags)?#x:""
#define SENT_OR_NOT(x) (FLAGS_SENT_##x & mhlTxConfig.miscFlags)?#x:""

void	SiiMhlTxMscCommandDone(uint8_t data1)
{
	TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone. data1 = %02X\n",
		(int)__LINE__, (int) data1));

	DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
		if (MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand) {
			if (MHL_DEV_CATEGORY_OFFSET == mhlTxConfig.mscLastOffset) {
				mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_CATEGORY;
				TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_CATEGORY,data1=0x%02x,vbusPowerState=0x%02x,(bool_t) (data1 & MHL_DEV_CATEGORY_POW_BIT)=0x%02x\n",
							(int)__LINE__, data1, vbusPowerState, (bool_t)(data1 & MHL_DEV_CATEGORY_POW_BIT)));

#if (VBUS_POWER_CHK == ENABLE)
				if (vbusPowerState != (bool_t)(data1 & MHL_DEV_CATEGORY_POW_BIT)) {
					vbusPowerState = (bool_t)(data1 & MHL_DEV_CATEGORY_POW_BIT);
					AppVbusControl(vbusPowerState);
				}
#endif

				SiiMhlTxReadDevcap(MHL_DEV_FEATURE_FLAG_OFFSET);
			} else if (MHL_DEV_FEATURE_FLAG_OFFSET == mhlTxConfig.mscLastOffset) {
				mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_FEATURE_FLAGS;
				TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_FEATURE_FLAGS\n", (int)__LINE__));

				mhlTxConfig.mscFeatureFlag = data1;

				mhlTxConfig.mscLastCommand = 0;
				mhlTxConfig.mscLastOffset  = 0;

				TX_DEBUG_PRINT(("[MHL]:%d Peer's Feature Flag = %02X\n\n", (int)__LINE__, (int)data1));
			}
		} else if (MHL_WRITE_STAT == mhlTxConfig.mscLastCommand) {

			TX_DEBUG_PRINT(("[MHL]: WRITE_STAT miscFlags: %02X\n\n", (int)mhlTxConfig.miscFlags));
			if (MHL_STATUS_REG_CONNECTED_RDY == mhlTxConfig.mscLastOffset) {
				if (MHL_STATUS_DCAP_RDY & mhlTxConfig.mscLastData) {
					mhlTxConfig.miscFlags |= FLAGS_SENT_DCAP_RDY;
					TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_SENT_DCAP_RDY\n", (int)__LINE__));
				}
			} else if (MHL_STATUS_REG_LINK_MODE == mhlTxConfig.mscLastOffset) {
				if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.mscLastData) {
					mhlTxConfig.miscFlags |= FLAGS_SENT_PATH_EN;
					TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_SENT_PATH_EN\n", (int)__LINE__));
				}
			}

			mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscLastOffset  = 0;
		} else if (MHL_MSC_MSG == mhlTxConfig.mscLastCommand) {
			if (MHL_MSC_MSG_RCPE == mhlTxConfig.mscMsgLastCommand) {
				if (SiiMhlTxRcpkSend(mhlTxConfig.mscSaveRcpKeyCode)) {
				}
			} else {
				/*TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone default\n"
				  "\tmscLastCommand: 0x%02X \n"
				  "\tmscMsgLastCommand: 0x%02X mscMsgLastData: 0x%02X\n"
				  "\tcbusReferenceCount: %d\n"
				  ,(int)__LINE__
				  ,(int)mhlTxConfig.mscLastCommand
				  ,(int)mhlTxConfig.mscMsgLastCommand
				  ,(int)mhlTxConfig.mscMsgLastData
				  ,(int)mhlTxConfig.cbusReferenceCount
				 ));*/
			}
			mhlTxConfig.mscLastCommand = 0;
		} else if (MHL_WRITE_BURST == mhlTxConfig.mscLastCommand) {
			TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone MHL_WRITE_BURST\n", (int)__LINE__));
			mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscLastOffset  = 0;
			mhlTxConfig.mscLastData    = 0;

			SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_DSCR_CHG, 0);
		} else if (MHL_SET_INT == mhlTxConfig.mscLastCommand) {
			TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone MHL_SET_INT\n", (int)__LINE__));
			if (MHL_RCHANGE_INT == mhlTxConfig.mscLastOffset) {
				TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone MHL_RCHANGE_INT\n", (int)__LINE__));
				if (MHL_INT_DSCR_CHG == mhlTxConfig.mscLastData)
				{
					DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
					TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone MHL_INT_DSCR_CHG\n", (int)__LINE__));
					ClrMiscFlag(SiiMhlTxMscCommandDone, FLAGS_SCRATCHPAD_BUSY)
				}
			}
			mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscLastOffset  = 0;
			mhlTxConfig.mscLastData    = 0;
		} else {
			/* TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone default\n"
			   "\tmscLastCommand: 0x%02X mscLastOffset: 0x%02X\n"
			   "\tcbusReferenceCount: %d\n"
			   ,(int)__LINE__
			   ,(int)mhlTxConfig.mscLastCommand
			   ,(int)mhlTxConfig.mscLastOffset
			   ,(int)mhlTxConfig.cbusReferenceCount
			  ));*/
		}
	if (!(FLAGS_RCP_READY & mhlTxConfig.miscFlags)) {
		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone. have(%s %s) sent(%s %s)\n"
					, (int) __LINE__
					, FLAG_OR_NOT(DEV_CATEGORY)
					, FLAG_OR_NOT(DEV_FEATURE_FLAGS)
					, SENT_OR_NOT(PATH_EN)
					, SENT_OR_NOT(DCAP_RDY)
				));
		if (FLAGS_HAVE_DEV_CATEGORY & mhlTxConfig.miscFlags) {
			if (FLAGS_HAVE_DEV_FEATURE_FLAGS & mhlTxConfig.miscFlags) {
				if (FLAGS_SENT_PATH_EN & mhlTxConfig.miscFlags) {
					if (FLAGS_SENT_DCAP_RDY & mhlTxConfig.miscFlags) {
						mhlTxConfig.miscFlags |= FLAGS_RCP_READY;
						mhlTxConfig.mhlConnectionEvent = true;
						mhlTxConfig.mhlConnected = MHL_TX_EVENT_RCP_READY;
					}
				}
			}
		}
	}
}
void SiiMhlTxMscWriteBurstDone(uint8_t data1)
{
#define WRITE_BURST_TEST_SIZE 16
	uint8_t temp[WRITE_BURST_TEST_SIZE];
	uint8_t i;
	TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscWriteBurstDone(%02X) \"",
		(int)__LINE__, (int)data1));
	SiiMhlTxDrvGetScratchPad(0, temp, WRITE_BURST_TEST_SIZE);
	for (i = 0; i < WRITE_BURST_TEST_SIZE ; ++i) {
		if (temp[i] >= ' ')
			TX_DEBUG_PRINT(("%02X %c ", (int)temp[i], temp[i]));
		else
			TX_DEBUG_PRINT(("%02X . ", (int)temp[i]));
	}
	TX_DEBUG_PRINT(("\"\n"));
}

void SiiMhlTxGotMhlMscMsg(uint8_t subCommand, uint8_t cmdData)
{
	mhlTxConfig.mscMsgArrived		= true;
	mhlTxConfig.mscMsgSubCommand	= subCommand;
	mhlTxConfig.mscMsgData			= cmdData;
}

void SiiMhlTxGotMhlIntr(uint8_t intr_0, uint8_t intr_1)
{
	TX_DEBUG_PRINT(("[MHL]: INTERRUPT Arrived. %02X, %02X\n", (int) intr_0, (int) intr_1));

	if (MHL_INT_DCAP_CHG & intr_0)
		SiiMhlTxReadDevcap(MHL_DEV_CATEGORY_OFFSET);

	if (MHL_INT_DSCR_CHG & intr_0) {
		SiiMhlTxDrvGetScratchPad(0, mhlTxConfig.localScratchPad,
			sizeof(mhlTxConfig.localScratchPad));
		ClrMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
	}
	if (MHL_INT_REQ_WRT & intr_0) {

		if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
			SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 1);
		else {
			SetMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
			SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 0);
		}
	}
	if (MHL_INT_GRT_WRT & intr_0) {
		uint8_t length = sizeof(mhlTxConfig.localScratchPad);
		TX_DEBUG_PRINT(("[MHL]:%d MHL_INT_GRT_WRT length:%d\n",
			(int)__LINE__, (int)length));
		SiiMhlTxDoWriteBurst(0x40, mhlTxConfig.localScratchPad, length);
	}

	if (MHL_INT_EDID_CHG & intr_1)
		SiiMhlTxDrvNotifyEdidChange();
}

void SiiMhlTxGotMhlStatus(uint8_t status_0, uint8_t status_1)
{
	uint8_t StatusChangeBitMask0, StatusChangeBitMask1;
	StatusChangeBitMask0 = status_0 ^ mhlTxConfig.status_0;
	StatusChangeBitMask1 = status_1 ^ mhlTxConfig.status_1;
	mhlTxConfig.status_0 = status_0;
	mhlTxConfig.status_1 = status_1;

	if (MHL_STATUS_DCAP_RDY & StatusChangeBitMask0) {
		TX_DEBUG_PRINT(("[MHL]: DCAP_RDY changed\n"));
		if (MHL_STATUS_DCAP_RDY & status_0)
			SiiMhlTxReadDevcap(MHL_DEV_CATEGORY_OFFSET);
	}

	if (MHL_STATUS_PATH_ENABLED & StatusChangeBitMask1) {
		TX_DEBUG_PRINT(("[MHL]: PATH_EN changed\n"));
		if (MHL_STATUS_PATH_ENABLED & status_1)
			SiiMhlTxSetPathEn();
		else
			SiiMhlTxClrPathEn();
	}
}

bool_t SiiMhlTxRcpSend(uint8_t rcpKeyCode)
{
	bool_t retVal;

	if ((0 == (MHL_FEATURE_RCP_SUPPORT & mhlTxConfig.mscFeatureFlag))
			||
			!(FLAGS_RCP_READY & mhlTxConfig.miscFlags)
	 ) {
		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxRcpSend failed\n", (int)__LINE__));
		retVal = false;
	}

	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCP, rcpKeyCode);
	if (retVal) {
		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxRcpSend\n", (int)__LINE__));
		IncrementCBusReferenceCount(SiiMhlTxRcpSend)
			MhlTxDriveStates();
	}
	return retVal;
}

bool_t SiiMhlTxRcpkSend(uint8_t rcpKeyCode)
{
	bool_t	retVal;

	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCPK, rcpKeyCode);
	if (retVal)
		MhlTxDriveStates();
	return retVal;
}

static bool_t SiiMhlTxRapkSend(void)
{
	return MhlTxSendMscMsg(MHL_MSC_MSG_RAPK, 0);
}

bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode)
{
	bool_t	retVal;

	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCPE, rcpeErrorCode);
	if (retVal)
		MhlTxDriveStates();
	return retVal;
}

static bool_t SiiMhlTxSetStatus(uint8_t regToWrite, uint8_t value)
{
	cbus_req_t	req;
	bool_t retVal;

	req.retryCount  = 2;
	req.command     = MHL_WRITE_STAT;
	req.offsetData  = regToWrite;
	req.payload_u.msgData[0]  = value;

	TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxSetStatus\n", (int)__LINE__));
	retVal = PutNextCBusTransaction(&req);
	return retVal;
}

static bool_t SiiMhlTxSetDCapRdy(void)
{
	mhlTxConfig.connectedReady |= MHL_STATUS_DCAP_RDY;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_CONNECTED_RDY, mhlTxConfig.connectedReady);
}

static bool_t SiiMhlTxSendLinkMode(void)
{
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

static bool_t SiiMhlTxSetPathEn(void)
{
	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxSetPathEn\n"));
	SiiMhlTxTmdsEnable();
	mhlTxConfig.linkMode |= MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

static bool_t SiiMhlTxClrPathEn(void)
{
	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxClrPathEn\n"));
	SiiMhlTxDrvTmdsControl(false);
	mhlTxConfig.linkMode &= ~MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

bool_t SiiMhlTxReadDevcap(uint8_t offset)
{
	cbus_req_t	req;
	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxReadDevcap\n"));
	req.retryCount  = 2;
	req.command     = MHL_READ_DEVCAP;
	req.offsetData  = offset;
	req.payload_u.msgData[0]  = 0;

	return PutNextCBusTransaction(&req);
}

static bool_t MhlTxSendMscMsg(uint8_t command, uint8_t cmdData)
{
	cbus_req_t	req;
	uint8_t		ccode;

	req.retryCount  = 2;
	req.command     = MHL_MSC_MSG;
	req.payload_u.msgData[0]  = command;
	req.payload_u.msgData[1]  = cmdData;
	ccode = PutNextCBusTransaction(&req);
	return (bool_t)ccode;
}

void SiiMhlTxNotifyConnection(bool_t mhlConnected)
{
	mhlTxConfig.mhlConnectionEvent = true;

	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxNotifyConnection MSC_STATE_IDLE %01X\n", (int) mhlConnected));

	if (mhlConnected) {
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_CONNECTION;
		mhlTxConfig.mhlHpdRSENflags |= MHL_RSEN;
		SiiMhlTxTmdsEnable();
		SiiMhlTxSendLinkMode();
	} else {
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_DISCONNECTION;
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_RSEN;
	}
}

void	SiiMhlTxNotifyDsHpdChange(uint8_t dsHpdStatus)
{
	if (0 == dsHpdStatus) {
		TX_DEBUG_PRINT(("[MHL]: Disable TMDS\n"));
		TX_DEBUG_PRINT(("[MHL]: DsHPD OFF\n"));
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_HPD;
		SiiMhlTxDrvTmdsControl(false);
	} else {
		TX_DEBUG_PRINT(("[MHL]: Enable TMDS\n"));
		TX_DEBUG_PRINT(("[MHL]: DsHPD ON\n"));
		mhlTxConfig.mhlHpdRSENflags |= MHL_HPD;
		SiiMhlTxTmdsEnable();
	}
}

static void	MhlTxResetStates(void)
{
	mhlTxConfig.mhlConnectionEvent	= false;
	mhlTxConfig.mhlConnected			= MHL_TX_EVENT_DISCONNECTION;
	mhlTxConfig.mhlHpdRSENflags &= ~(MHL_RSEN | MHL_HPD);
	mhlTxConfig.mscMsgArrived		= false;

	mhlTxConfig.status_0		= 0;
	mhlTxConfig.status_1		= 0;
	mhlTxConfig.connectedReady	= 0;
	mhlTxConfig.linkMode		= 3;
	mhlTxConfig.cbusReferenceCount	= 0;
	mhlTxConfig.miscFlags		= 0;
	mhlTxConfig.mscLastCommand	= 0;
	mhlTxConfig.mscMsgLastCommand	= 0;
}

#if (VBUS_POWER_CHK == ENABLE)
void MHLSinkOrDonglePowerStatusCheck(void)
{
	uint8_t RegValue;

	if (POWER_STATE_D0_MHL == fwPowerState) {
		WriteByteCBUS(REG_CBUS_PRI_ADDR_CMD, MHL_DEV_CATEGORY_OFFSET);
		WriteByteCBUS(REG_CBUS_PRI_START, MSC_START_BIT_READ_REG);

		RegValue = ReadByteCBUS(REG_CBUS_PRI_RD_DATA_1ST);
		TX_DEBUG_PRINT(("[MHL]: Device Category register=0x%02X...\n", (int)RegValue));

		if (MHL_DEV_CAT_UNPOWERED_DONGLE == (RegValue & 0x0F))
			TX_DEBUG_PRINT(("[MHL]: DevTypeValue=0x%02X, limit the VBUS "
				"current input from dongle to be 100mA...\n", (int)RegValue));
		else if (MHL_DEV_CAT_SINGLE_INPUT_SINK == (RegValue & 0x0F))
			TX_DEBUG_PRINT(("[MHL]: DevTypeValue=0x%02X, limit the VBUS "
				"current input from sink to be 500mA...\n", (int)RegValue));
	}
}



void MHLPowerStatusCheck(void)
{
	static uint8_t DevCatPOWValue;
	uint8_t RegValue;

	if (POWER_STATE_D0_MHL == fwPowerState) {
		WriteByteCBUS(REG_CBUS_PRI_ADDR_CMD, MHL_DEV_CATEGORY_OFFSET);
		WriteByteCBUS(REG_CBUS_PRI_START, MSC_START_BIT_READ_REG);

		RegValue = ReadByteCBUS(REG_CBUS_PRI_RD_DATA_1ST);

		if (DevCatPOWValue != (RegValue & MHL_DEV_CATEGORY_POW_BIT)) {
			DevCatPOWValue = RegValue & MHL_DEV_CATEGORY_POW_BIT;
			TX_DEBUG_PRINT(("[MHL]: DevCapReg0x02=0x%02X, POW bit Changed...\n", (int)RegValue));

			if (vbusPowerState != (bool_t) (DevCatPOWValue)) {
				vbusPowerState = (bool_t) (DevCatPOWValue);
				AppVbusControl(vbusPowerState);
			}
		}
	}
}
#endif
