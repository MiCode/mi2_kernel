/***********************************************************************************/
/*  Copyright (c) 2002-2009, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

#include "si_mhl_defs.h"
#include "sii_reg_access.h"

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
