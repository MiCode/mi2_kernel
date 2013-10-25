/***********************************************************************************/
/*  Copyright (c) 2010-2011, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
/*
@file: si_mhl_tx_base_drv_api.h
 */
typedef struct
{
    uint8_t reqStatus;
    uint8_t retryCount;
    uint8_t command;
    uint8_t offsetData;
    uint8_t length;
    union {
        uint8_t msgData[16];
	unsigned char *pdatabytes;
    } payload_u;

} cbus_req_t;

bool_t 	SiiMhlTxChipInitialize(void);


void 	SiiMhlTxDeviceIsr(void);

bool_t	SiiMhlTxDrvSendCbusCommand(cbus_req_t *pReq);

bool_t SiiMhlTxDrvCBusBusy(void);

void	SiiMhlTxDrvTmdsControl(bool_t enable);


void	SiiMhlTxDrvNotifyEdidChange(void);
bool_t SiiMhlTxReadDevcap(uint8_t offset);

void SiiMhlTxDrvGetScratchPad(uint8_t startReg,uint8_t *pData,uint8_t length);


/*
	SiMhlTxDrvSetClkMode
	-- Set the hardware this this clock mode.
 */
void SiMhlTxDrvSetClkMode(uint8_t clkMode);


extern	void	SiiMhlTxNotifyDsHpdChange(uint8_t dsHpdStatus);
extern	void	SiiMhlTxNotifyConnection(bool_t mhlConnected);

/*
	SiiMhlTxNotifyRgndMhl
	The driver calls this when it has determined that the peer device is an MHL device.
	This routine will give the application layer the first crack at handling VBUS power
	at this point in the discovery process.
 */
extern void SiiMhlTxNotifyRgndMhl(void);

extern	void	SiiMhlTxMscCommandDone(uint8_t data1);
extern	void	SiiMhlTxMscWriteBurstDone(uint8_t data1);
extern	void	SiiMhlTxGotMhlIntr(uint8_t intr_0, uint8_t intr_1);
extern	void	SiiMhlTxGotMhlStatus(uint8_t status_0, uint8_t status_1);
extern	void	SiiMhlTxGotMhlMscMsg(uint8_t subCommand, uint8_t cmdData);
extern	void	SiiMhlTxGotMhlWriteBurst(uint8_t *spadArray);

/*
	SiiMhlTxDrvProcessRgndMhl
		optionally called by the MHL Tx Component after giving the OEM layer the
		first crack at handling the event.
*/
extern void SiiMhlTxDrvProcessRgndMhl(void);

