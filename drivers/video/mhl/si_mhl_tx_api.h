/**********************************************************************************/
/*  Copyright (c) 2011, Silicon Image, Inc.  All rights reserved.                 */
/*  No part of this work may be reproduced, modified, distributed, transmitted,   */
/*  transcribed, or translated into any language or computer format, in any form  */
/*  or by any means without written permission of: Silicon Image, Inc.,           */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                          */
/**********************************************************************************/
/*
   @file si_mhl_tx_api.h
 */
#include<linux/kernel.h>
#include "sii_9244_api.h"

bool_t SiiMhlTxInitialize( uint8_t pollIntervalMs );

#define	MHL_TX_EVENT_NONE				0x00	/* No event worth reporting.  */
#define	MHL_TX_EVENT_DISCONNECTION		0x01	/* MHL connection has been lost */
#define	MHL_TX_EVENT_CONNECTION			0x02	/* MHL connection has been established */
#define	MHL_TX_EVENT_RCP_READY			0x03	/* MHL connection is ready for RCP */
#define	MHL_TX_EVENT_RCP_RECEIVED		0x04	/* Received an RCP. Key Code in "eventParameter" */
#define	MHL_TX_EVENT_RCPK_RECEIVED		0x05	/* Received an RCPK message */
#define	MHL_TX_EVENT_RCPE_RECEIVED		0x06	/* Received an RCPE message .*/
#define	MHL_TX_EVENT_DCAP_CHG			0x07	/* Received DCAP_CHG interrupt */
#define	MHL_TX_EVENT_DSCR_CHG			0x08	/* Received DSCR_CHG interrupt */
#define	MHL_TX_EVENT_POW_BIT_CHG		0x09	/* Peer's power capability has changed */
#define	MHL_TX_EVENT_RGND_MHL			0x0A	/* RGND measurement has determine that the peer is an MHL device */

typedef enum {
	MHL_TX_EVENT_STATUS_HANDLED = 0,
	MHL_TX_EVENT_STATUS_PASSTHROUGH
} MhlTxNotifyEventsStatus_e;
bool_t SiiMhlTxRcpSend(uint8_t rcpKeyCode);

bool_t SiiMhlTxRcpkSend(uint8_t rcpKeyCode);

bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode);

bool_t SiiMhlTxSetPathEn(void);

bool_t SiiMhlTxClrPathEn(void);

extern	void	AppMhlTxDisableInterrupts(void);

extern	void	AppMhlTxRestoreInterrupts(void);

extern	void	AppVbusControl(bool_t powerOn);

void  AppNotifyMhlEnabledStatusChange(bool_t enabled);

void  AppNotifyMhlDownStreamHPDStatusChange(bool_t connected);

MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam);

/*

	AppResetMhlTx
		- reset the chip in board dependent fashion
 */

typedef enum {
	SCRATCHPAD_FAIL= -4
	,SCRATCHPAD_BAD_PARAM = -3
	,SCRATCHPAD_NOT_SUPPORTED = -2
	,SCRATCHPAD_BUSY = -1
	,SCRATCHPAD_SUCCESS = 0
} ScratchPadStatus_e;

ScratchPadStatus_e SiiMhlTxRequestWriteBurst(uint8_t startReg, uint8_t length, uint8_t *pData);

bool_t MhlTxCBusBusy(void);

void MhlTxProcessEvents(void);

uint8_t    SiiTxReadConnectionStatus(void);


/*
  SiiMhlTxSetPreferredPixelFormat

	clkMode - the preferred pixel format for the CLK_MODE status register

	Returns: 0 -- success
		     1 -- failure - bits were specified that are not within the mask
 */
uint8_t SiiMhlTxSetPreferredPixelFormat(uint8_t clkMode);

/*
	SiiTxGetPeerDevCapEntry

	Parameters:
		index -- the devcap index to get
		*pData pointer to location to write data
	returns
		0 -- success
		1 -- busy.
 */
uint8_t SiiTxGetPeerDevCapEntry(uint8_t index,uint8_t *pData);

/*
	SiiGetScratchPadVector

	Parameters:
		offset -- The beginning offset into the scratch pad from which to fetch entries.
		length -- The number of entries to fetch
		*pData -- A pointer to an array of bytes where the data should be placed.

	returns:
		0 -- success
		< 0  -- error (negative numbers)

*/
ScratchPadStatus_e SiiGetScratchPadVector(uint8_t offset,uint8_t length, uint8_t *pData);

void MHLSinkOrDonglePowerStatusCheck(void);


/*
	SiiMhlTxHwReset
		This routine percolates the call to reset the Mhl Tx chip up to the application layer.
 */
 #define ENABLE_TX_DEBUG_PRINT 1

#define DISABLE 0x00
#define ENABLE  0xFF

#define CONF__TX_API_PRINT   	(ENABLE)
#define CONF__TX_DEBUG_PRINT    (DISABLE)
/*\
| | Debug Print Macro
| |
| | Note: TX_DEBUG_PRINT Requires double parenthesis
| | Example:  TX_DEBUG_PRINT(("hello, world!\n"));
\*/

#if (CONF__TX_DEBUG_PRINT == ENABLE)
    #define TX_DEBUG_PRINT(x)	printk x
#else
    #define TX_DEBUG_PRINT(x)
#endif

#if (CONF__TX_API_PRINT == ENABLE)
    #define TX_API_PRINT(x)	printk x
#else
    #define TX_API_PRINT(x)
#endif

