/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_renesas.h"
#include <mach/socinfo.h>
#if defined(CONFIG_LEDS_LM3530)
#include <linux/led-lm3530.h>
#endif

#define DISP_P1
#define CE_HITACHI_MODE3

#define RENESAS_NO_DELAY 0	/* 0 */
#define RENESAS_CMD_DELAY 50	/* 50 default */
#define RENESAS_CMD_DELAY_10 10 /* 10 */
#define RENESAS_CMD_DELAY_20 20 /* 20 */
#define RENESAS_CMD_DELAY_60 20 /* 60 */
#define RENESAS_SLEEP_OFF_DELAY 120
static struct msm_panel_common_pdata *mipi_renesas_pdata;

static struct dsi_buf renesas_tx_buf;
static struct dsi_buf renesas_rx_buf;

static int mipi_renesas_lcd_init(void);

static int dispparam;	/* record param for display on */

/*[3] exit sleep mode*/
static char exit_sleep_mode[2] = {0x11, 0x00}; /*DTYPE_DCS_WRITE*/
static char mcap_start[2] = {0xb0, 0x04};	/*DTYPE_GEN_WRITE2*/
#if defined(DISP_P1)
static char nvm_noload[2] = {0xd6, 0x01};	/*DTYPE_GEN_WRITE2*/	/*Sharp only*/
static char disp_setting[2] = {0xc1, 0x04};	/*DTYPE_GEN_WRITE2*/	/*Sharp only*/
#endif

#if defined(CONFIG_FB_MSM_MIPI_DSI_CABC)
#if defined(CABC_DEFAULT)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x04, 0x00,
	0x0C, 0x14, 0xAC, 0x14,	0x6C, 0x14, 0x0C, 0x14,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x37, 0x5A, 0x87,
	0xBE, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x1A, 0x18, 0x02, 0x40, 0x85, 0x0A, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x1A, 0x18, 0x02, 0x40, 0x85, 0x00, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION1)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x04, 0x00,
	0x0C, 0x14, 0xAC, 0x14,	0x6C, 0x14, 0x0C, 0x14,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x37, 0x5A, 0x87,
	0xBE, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION2)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x04, 0x00,
	0x0C, 0x12, 0x6C, 0x12,	0xAC, 0x12, 0x0C, 0x12,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x67, 0x89, 0xAF,
	0xD6, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION3)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x04, 0x00,
	0x0C, 0x10, 0x6C, 0x10,	0xAC, 0x10, 0x0C, 0x10,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x8C, 0xAA, 0xC7,
	0xE3, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION4)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x04, 0x00,
	0x0C, 0x0C, 0x6C, 0x0C,	0xAC, 0x0C, 0x0C, 0x0C,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0xB3, 0xC9, 0xDC,
	0xEE, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x1A, 0x18, 0x04, 0x40, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION5)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x1F, 0x00,
	0x0C, 0x0F, 0x6C, 0x0F,	0x6C, 0x0F, 0x0C, 0x0F,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x67, 0x89, 0xAF,
	0xD6, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x00, 0x3F, 0x04, 0x40, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x00, 0x3F, 0x04, 0x40, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#elif defined(CABC_OPTION6)
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x1F, 0x00,
	0x0C, 0x12, 0x6C, 0x11,	0x6C, 0x12, 0x0C, 0x12,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x67, 0xA3, 0xDB,
	0xFB, 0xFF};		/*DTYPE_GEN_LWRITE*/
static char cabc_movie_still[8] = {
	0xB9, 0x00, 0x30, 0x18, 0x18, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x00, 0x30, 0x18, 0x18, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#else
static char test[26] = {
	0xB8, 0x18, 0x80, 0x18, 0x18, 0xCF, 0x1F, 0x00,
	0x0C, 0x0E, 0x6C, 0x0E,	0x6C, 0x0E, 0x0C, 0x0E,
	0xDA, 0x6D, 0xFF, 0xFF,	0x10, 0x8C, 0xD2, 0xFF,
	0xFF, 0xFF};		/*DTYPE_GEN_LWRITE*/		/* Set CABC */
static char cabc_movie_still[8] = {
	0xB9, 0x00, 0x3F, 0x18, 0x18, 0x9F, 0x1F, 0x80};	/*DTYPE_GEN_LWRITE*/
static char cabc_user_inf[8] = {
	0xBA, 0x00, 0x3F, 0x18, 0x18, 0x9F, 0x1F, 0xD7};	/*DTYPE_GEN_LWRITE*/
#endif
#endif

static char mcap_end[2] = {0xb0, 0x03};					/*DTYPE_GEN_WRITE2*/
#if defined(CONFIG_FB_MSM_MIPI_DSI_CABC)
static char write_display_brightness[3] = {0x51, 0xE, 0xFF};		/* DTYPE_DCS_LWRITE */
static char write_cabc[2] = {0x55, 0x01};				/* DTYPE_DCS_WRITE1 */
static char write_control_display[2] = {0x53, 0x2C};			/* DTYPE_DCS_WRITE1 */
#else
static char write_display_brightness[3] = {0x51, 0xE, 0xFF};		/* DTYPE_DCS_LWRITE */
static char write_cabc[2] = {0x55, 0x00};				/* DTYPE_DCS_WRITE1 */
static char write_control_display[2] = {0x53, 0x00};			/* DTYPE_DCS_WRITE1 */
#endif

#if defined(CONFIG_FB_MSM_MIPI_DSI_CE)
#if defined(CE_HITACHI_MODE1)
static char write_ce_on_jdi[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x0C, 0x3F, 0x14, 0x80, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_HITACHI_MODE2)
static char write_ce_on_jdi[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x18, 0x3F, 0x14, 0x8A, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x19, 0xD6, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_HITACHI_MODE3)
static char write_ce_on_jdi[33] = {
		0xCA, 0x01, 0x80, 0x88, 0x8C, 0xBC, 0x8C, 0x8C,
		0x8C, 0x18, 0x3F, 0x14, 0xFF, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_HITACHI_MODE4)
static char write_ce_on_jdi[33] = {
		0xCA, 0x01, 0x80, 0x8A, 0x8C, 0xDC, 0x96, 0x96,
		0x90, 0x18, 0x3F, 0x14, 0xFF, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#else
static char write_ce_on_jdi[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x0C, 0x3F, 0x14, 0x80, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#endif
#if defined(CE_SHARP_EXAMPLE1)
static char write_ce_on[33] = {
		0xCA, 0x01, 0x80, 0x83, 0xA5, 0xC8, 0xD2, 0xDC,
		0xDC, 0x08, 0x20, 0x80, 0xFF, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_SHARP_EXAMPLE2)
static char write_ce_on[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x0C, 0x3F, 0x14, 0x80, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_SHARP_EXAMPLE3)
static char write_ce_on[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x18, 0x3F, 0x14, 0x8A, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#elif defined(CE_SHARP_EXAMPLE4)
static char write_ce_on[33] = {
		0xCA, 0x01, 0x80, 0xDC, 0xF0, 0xDC, 0xF0, 0xDC,
		0xF0, 0x18, 0x3F, 0x14, 0x8A, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x19, 0xD6, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#else
static char write_ce_on[33] = {
		0xCA, 0x01, 0x80, 0x8A, 0x8C, 0xC8, 0x8C, 0x80,
		0x8C, 0x18, 0x3F, 0x14, 0xFF, 0x0A, 0x4A, 0x37,
		0xA0, 0x55, 0xF8, 0x0C, 0x0C, 0x20, 0x10, 0x3F,
		0x3F, 0x00, 0x00, 0x10, 0x10, 0x3F, 0x3F, 0x3F,
		0x3F};				/* DTYPE_GEN_LWRITE */
#endif
#endif
static char set_tear_on[2] = {0x35, 0x00};				/*DTYPE_DCS_WRITE1*/
/*Cmd for GRAM Access*/
static char set_column_address[5] = {0x2A, 0x00, 0x00, 0x02, 0xCF}; /* DTYPE_DCS_LWRITE */
static char set_page1_address[5] = {0x2B, 0x00, 0x00, 0x04, 0xFF }; /* DTYPE_DCS_LWRITE */

/*[4] enter sleep mode*/
static char set_tear_off[2] = {0x34, 0x00};				/*DTYPE_DCS_WRITE*/
static char enter_sleep_mode[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */

/*[5] set display on*/
static char set_address_mode[2] = {0x36, 0x00}; /* DTYPE_DCS_WRITE1 */
static char set_pixel_format[2] = {0x3a, 0x77}; /* DTYPE_DCS_WRITE1 */
static char set_display_on[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/*[6] set display off*/
static char set_display_off[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */

/*[7] enter deep standby*/

/*[8] exit deep standby*/

/*[9] enter video Mode*/
static char set_interface_video[2] = {0xB3, 0x40};		/*DTYPE_GEN_WRITE2*/

/*[10] enter command mode*/
static char set_interface_cmd1[2] = {0xB3, 0xA0};		/*DTYPE_GEN_WRITE2*/
static char set_interface_cmd2[2] = {0xB3, 0x00};		/*DTYPE_GEN_WRITE2*/

static char config_sleep_out[2] = {0x11, 0x00};
static char config_CMD_MODE[2] = {0x40, 0x01};
static char config_WRTXHT[7] = {0x92, 0x16, 0x08, 0x08, 0x00, 0x01, 0xe0};
static char config_WRTXVT[7] = {0x8b, 0x02, 0x02, 0x02, 0x00, 0x03, 0x60};
static char config_PLL2NR[2] = {0xa0, 0x24};
static char config_PLL2NF1[2] = {0xa2, 0xd0};
static char config_PLL2NF2[2] = {0xa4, 0x00};
static char config_PLL2BWADJ1[2] = {0xa6, 0xd0};
static char config_PLL2BWADJ2[2] = {0xa8, 0x00};
static char config_PLL2CTL[2] = {0xaa, 0x00};
static char config_DBICBR[2] = {0x48, 0x03};
static char config_DBICTYPE[2] = {0x49, 0x00};
static char config_DBICSET1[2] = {0x4a, 0x1c};
static char config_DBICADD[2] = {0x4b, 0x00};
static char config_DBICCTL[2] = {0x4e, 0x01};
/* static char config_COLMOD_565[2] = {0x3a, 0x05}; */
/* static char config_COLMOD_666PACK[2] = {0x3a, 0x06}; */
static char config_COLMOD_888[2] = {0x3a, 0x07};
static char config_MADCTL[2] = {0x36, 0x00};
static char config_DBIOC[2] = {0x82, 0x40};
static char config_CASET[7] = {0x2a, 0x00, 0x00, 0x00, 0x00, 0x01, 0xdf };
static char config_PASET[7] = {0x2b, 0x00, 0x00, 0x00, 0x00, 0x03, 0x5f };
static char config_TXON[2] = {0x81, 0x00};
static char config_BLSET_TM[2] = {0xff, 0x6c};
static char config_DSIRXCTL[2] = {0x41, 0x01};
static char config_TEON[2] = {0x35, 0x00};
static char config_TEOFF[1] = {0x34};

static char config_AGCPSCTL_TM[2] = {0x56, 0x08};

static char config_DBICADD70[2] = {0x4b, 0x70};
static char config_DBICSET_15[2] = {0x4a, 0x15};
static char config_DBICADD72[2] = {0x4b, 0x72};

static char config_Power_Ctrl_2a_cmd[3] = {0x4c, 0x40, 0x10};
static char config_Auto_Sequencer_Setting_a_cmd[3] = {0x4c, 0x00, 0x00};
static char Driver_Output_Ctrl_indx[3] = {0x4c, 0x00, 0x01};
static char Driver_Output_Ctrl_cmd[3] = {0x4c, 0x03, 0x10};
static char config_LCD_drive_AC_Ctrl_indx[3] = {0x4c, 0x00, 0x02};
static char config_LCD_drive_AC_Ctrl_cmd[3] = {0x4c, 0x01, 0x00};
static char config_Entry_Mode_indx[3] = {0x4c, 0x00, 0x03};
static char config_Entry_Mode_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Display_Ctrl_1_indx[3] = {0x4c, 0x00, 0x07};
static char config_Display_Ctrl_1_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Display_Ctrl_2_indx[3] = {0x4c, 0x00, 0x08};
static char config_Display_Ctrl_2_cmd[3] = {0x4c, 0x00, 0x04};
static char config_Display_Ctrl_3_indx[3] = {0x4c, 0x00, 0x09};
static char config_Display_Ctrl_3_cmd[3] = {0x4c, 0x00, 0x0c};
static char config_Display_IF_Ctrl_1_indx[3] = {0x4c, 0x00, 0x0c};
static char config_Display_IF_Ctrl_1_cmd[3] = {0x4c, 0x40, 0x10};
static char config_Display_IF_Ctrl_2_indx[3] = {0x4c, 0x00, 0x0e};
static char config_Display_IF_Ctrl_2_cmd[3] = {0x4c, 0x00, 0x00};

static char config_Panel_IF_Ctrl_1_indx[3] = {0x4c, 0x00, 0x20};
static char config_Panel_IF_Ctrl_1_cmd[3] = {0x4c, 0x01, 0x3f};
static char config_Panel_IF_Ctrl_3_indx[3] = {0x4c, 0x00, 0x22};
static char config_Panel_IF_Ctrl_3_cmd[3] = {0x4c, 0x76, 0x00};
static char config_Panel_IF_Ctrl_4_indx[3] = {0x4c, 0x00, 0x23};
static char config_Panel_IF_Ctrl_4_cmd[3] = {0x4c, 0x1c, 0x0a};
static char config_Panel_IF_Ctrl_5_indx[3] = {0x4c, 0x00, 0x24};
static char config_Panel_IF_Ctrl_5_cmd[3] = {0x4c, 0x1c, 0x2c};
static char config_Panel_IF_Ctrl_6_indx[3] = {0x4c, 0x00, 0x25};
static char config_Panel_IF_Ctrl_6_cmd[3] = {0x4c, 0x1c, 0x4e};
static char config_Panel_IF_Ctrl_8_indx[3] = {0x4c, 0x00, 0x27};
static char config_Panel_IF_Ctrl_8_cmd[3] = {0x4c, 0x00, 0x00};
static char config_Panel_IF_Ctrl_9_indx[3] = {0x4c, 0x00, 0x28};
static char config_Panel_IF_Ctrl_9_cmd[3] = {0x4c, 0x76, 0x0c};


static char config_gam_adjust_00_indx[3] = {0x4c, 0x03, 0x00};
static char config_gam_adjust_00_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_01_indx[3] = {0x4c, 0x03, 0x01};
static char config_gam_adjust_01_cmd[3] = {0x4c, 0x05, 0x02};
static char config_gam_adjust_02_indx[3] = {0x4c, 0x03, 0x02};
static char config_gam_adjust_02_cmd[3] = {0x4c, 0x07, 0x05};
static char config_gam_adjust_03_indx[3] = {0x4c, 0x03, 0x03};
static char config_gam_adjust_03_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_04_indx[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_04_cmd[3] = {0x4c, 0x02, 0x00};
static char config_gam_adjust_05_indx[3] = {0x4c, 0x03, 0x05};
static char config_gam_adjust_05_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_06_indx[3] = {0x4c, 0x03, 0x06};
static char config_gam_adjust_06_cmd[3] = {0x4c, 0x10, 0x10};
static char config_gam_adjust_07_indx[3] = {0x4c, 0x03, 0x07};
static char config_gam_adjust_07_cmd[3] = {0x4c, 0x02, 0x02};
static char config_gam_adjust_08_indx[3] = {0x4c, 0x03, 0x08};
static char config_gam_adjust_08_cmd[3] = {0x4c, 0x07, 0x04};
static char config_gam_adjust_09_indx[3] = {0x4c, 0x03, 0x09};
static char config_gam_adjust_09_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_0A_indx[3] = {0x4c, 0x03, 0x0a};
static char config_gam_adjust_0A_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_0B_indx[3] = {0x4c, 0x03, 0x0b};
static char config_gam_adjust_0B_cmd[3] = {0x4c, 0x00, 0x00};
static char config_gam_adjust_0C_indx[3] = {0x4c, 0x03, 0x0c};
static char config_gam_adjust_0C_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_0D_indx[3] = {0x4c, 0x03, 0x0d};
static char config_gam_adjust_0D_cmd[3] = {0x4c, 0x10, 0x10};
static char config_gam_adjust_10_indx[3] = {0x4c, 0x03, 0x10};
static char config_gam_adjust_10_cmd[3] = {0x4c, 0x01, 0x04};
static char config_gam_adjust_11_indx[3] = {0x4c, 0x03, 0x11};
static char config_gam_adjust_11_cmd[3] = {0x4c, 0x05, 0x03};
static char config_gam_adjust_12_indx[3] = {0x4c, 0x03, 0x12};
static char config_gam_adjust_12_cmd[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_15_indx[3] = {0x4c, 0x03, 0x15};
static char config_gam_adjust_15_cmd[3] = {0x4c, 0x03, 0x04};
static char config_gam_adjust_16_indx[3] = {0x4c, 0x03, 0x16};
static char config_gam_adjust_16_cmd[3] = {0x4c, 0x03, 0x1c};
static char config_gam_adjust_17_indx[3] = {0x4c, 0x03, 0x17};
static char config_gam_adjust_17_cmd[3] = {0x4c, 0x02, 0x04};
static char config_gam_adjust_18_indx[3] = {0x4c, 0x03, 0x18};
static char config_gam_adjust_18_cmd[3] = {0x4c, 0x04, 0x02};
static char config_gam_adjust_19_indx[3] = {0x4c, 0x03, 0x19};
static char config_gam_adjust_19_cmd[3] = {0x4c, 0x03, 0x05};
static char config_gam_adjust_1C_indx[3] = {0x4c, 0x03, 0x1c};
static char config_gam_adjust_1C_cmd[3] = {0x4c, 0x07, 0x07};
static char config_gam_adjust_1D_indx[3] = {0x4c, 0x03, 0x1D};
static char config_gam_adjust_1D_cmd[3] = {0x4c, 0x02, 0x1f};
static char config_gam_adjust_20_indx[3] = {0x4c, 0x03, 0x20};
static char config_gam_adjust_20_cmd[3] = {0x4c, 0x05, 0x07};
static char config_gam_adjust_21_indx[3] = {0x4c, 0x03, 0x21};
static char config_gam_adjust_21_cmd[3] = {0x4c, 0x06, 0x04};
static char config_gam_adjust_22_indx[3] = {0x4c, 0x03, 0x22};
static char config_gam_adjust_22_cmd[3] = {0x4c, 0x04, 0x05};
static char config_gam_adjust_27_indx[3] = {0x4c, 0x03, 0x27};
static char config_gam_adjust_27_cmd[3] = {0x4c, 0x02, 0x03};
static char config_gam_adjust_28_indx[3] = {0x4c, 0x03, 0x28};
static char config_gam_adjust_28_cmd[3] = {0x4c, 0x03, 0x00};
static char config_gam_adjust_29_indx[3] = {0x4c, 0x03, 0x29};
static char config_gam_adjust_29_cmd[3] = {0x4c, 0x00, 0x02};

static char config_Power_Ctrl_1_indx[3] = {0x4c, 0x01, 0x00};
static char config_Power_Ctrl_1b_cmd[3] = {0x4c, 0x36, 0x3c};
static char config_Power_Ctrl_2_indx[3] = {0x4c, 0x01, 0x01};
static char config_Power_Ctrl_2b_cmd[3] = {0x4c, 0x40, 0x03};
static char config_Power_Ctrl_3_indx[3] = {0x4c, 0x01, 0x02};
static char config_Power_Ctrl_3a_cmd[3] = {0x4c, 0x00, 0x01};
static char config_Power_Ctrl_4_indx[3] = {0x4c, 0x01, 0x03};
static char config_Power_Ctrl_4a_cmd[3] = {0x4c, 0x3c, 0x58};
static char config_Power_Ctrl_6_indx[3] = {0x4c, 0x01, 0x0c};
static char config_Power_Ctrl_6a_cmd[3] = {0x4c, 0x01, 0x35};

static char config_Auto_Sequencer_Setting_b_cmd[3] = {0x4c, 0x00, 0x02};

static char config_Panel_IF_Ctrl_10_indx[3] = {0x4c, 0x00, 0x29};
static char config_Panel_IF_Ctrl_10a_cmd[3] = {0x4c, 0x03, 0xbf};
static char config_Auto_Sequencer_Setting_indx[3] = {0x4c, 0x01, 0x06};
static char config_Auto_Sequencer_Setting_c_cmd[3] = {0x4c, 0x00, 0x03};
static char config_Power_Ctrl_2c_cmd[3] = {0x4c, 0x40, 0x10};

static char config_VIDEO[2] = {0x40, 0x00};

static char config_Panel_IF_Ctrl_10_indx_off[3] = {0x4C, 0x00, 0x29};

static char config_Panel_IF_Ctrl_10b_cmd_off[3] = {0x4C, 0x00, 0x02};

static char config_Power_Ctrl_1a_cmd[3] = {0x4C, 0x30, 0x00};

static struct dsi_cmd_desc renesas_sleep_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_sleep_out), config_sleep_out }
};

static struct dsi_cmd_desc renesas_display_off_cmds[] = {
	/* Choosing Command Mode */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE },

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_b_cmd),
			config_Auto_Sequencer_Setting_b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY * 2,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	/* After waiting >= 5 frames, turn OFF RGB signals
	This is done by on DSI/MDP (depends on Vid/Cmd Mode.  */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_a_cmd),
			config_Auto_Sequencer_Setting_a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10_indx_off),
			config_Panel_IF_Ctrl_10_indx_off},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10b_cmd_off),
				config_Panel_IF_Ctrl_10b_cmd_off},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx),
				config_Power_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1a_cmd),
				config_Power_Ctrl_1a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TEOFF), config_TEOFF},
};

static struct dsi_cmd_desc renesas_hitachi_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(exit_sleep_mode), exit_sleep_mode },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_start), mcap_start },
#if defined(CONFIG_FB_MSM_MIPI_DSI_CE)
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_ce_on_jdi), write_ce_on_jdi },
#else
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_ce_off), write_ce_off },
#endif
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_end), mcap_end },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_display_brightness), write_display_brightness },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_control_display), write_control_display },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_cabc), write_cabc },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_column_address), set_column_address },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_page1_address), set_page1_address },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_address_mode), set_address_mode },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_pixel_format), set_pixel_format },
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_display_on), set_display_on },
};

static struct dsi_cmd_desc renesas_hitachi_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_display_off), set_display_off },
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_60,
		sizeof(enter_sleep_mode), enter_sleep_mode },
};

static struct dsi_cmd_desc renesas_sharp_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(exit_sleep_mode), exit_sleep_mode },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_start), mcap_start },
#if defined(DISP_P1)
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(nvm_noload), nvm_noload },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(disp_setting), disp_setting },
#endif
#if defined(CONFIG_FB_MSM_MIPI_DSI_CABC)
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(test), test },
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(cabc_movie_still), cabc_movie_still },
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(cabc_user_inf), cabc_user_inf },
#endif
#if defined(CONFIG_FB_MSM_MIPI_DSI_CE)
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_ce_on), write_ce_on },
#else
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_ce_off), write_ce_off },
#endif
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_end), mcap_end },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_display_brightness), write_display_brightness },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_control_display), write_control_display },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(write_cabc), write_cabc },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_column_address), set_column_address },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_page1_address), set_page1_address },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_address_mode), set_address_mode },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_pixel_format), set_pixel_format },
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_display_on), set_display_on },
};

static struct dsi_cmd_desc renesas_sharp_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_display_off), set_display_off },
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY_60,
		sizeof(enter_sleep_mode), enter_sleep_mode },
};

static struct dsi_cmd_desc renesas_videomode_on_cmds[] = {
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_start), mcap_start },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_interface_video), set_interface_video },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_end), mcap_end },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_tear_on), set_tear_on },
};

static struct dsi_cmd_desc renesas_videomode_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_tear_off), set_tear_off },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_start), mcap_start },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_CMD_DELAY_20,
		sizeof(set_interface_cmd1), set_interface_cmd1 },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(set_interface_cmd2), set_interface_cmd2 },
	{DTYPE_GEN_WRITE2, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(mcap_end), mcap_end },
};

static struct dsi_cmd_desc renesas_display_on_cmds[] = {
	/* Choosing Command Mode */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXHT), config_WRTXHT },
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXVT), config_WRTXVT },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NR), config_PLL2NR },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NF1), config_PLL2NF1 },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2NF2), config_PLL2NF2 },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2BWADJ1), config_PLL2BWADJ1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2BWADJ2), config_PLL2BWADJ2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PLL2CTL), config_PLL2CTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICBR), config_DBICBR},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICTYPE), config_DBICTYPE},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET1), config_DBICSET1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD), config_DBICADD},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICCTL), config_DBICCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_COLMOD_888), config_COLMOD_888},
	/* Choose config_COLMOD_565 or config_COLMOD_666PACK for other modes */
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_MADCTL), config_MADCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBIOC), config_DBIOC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CASET), config_CASET},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_PASET), config_PASET},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DSIRXCTL), config_DSIRXCTL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TEON), config_TEON},
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_TXON), config_TXON},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_BLSET_TM), config_BLSET_TM},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_AGCPSCTL_TM), config_AGCPSCTL_TM},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx), config_Power_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1a_cmd), config_Power_Ctrl_1a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx), config_Power_Ctrl_2_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2a_cmd), config_Power_Ctrl_2a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_a_cmd),
			config_Auto_Sequencer_Setting_a_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(Driver_Output_Ctrl_indx), Driver_Output_Ctrl_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(Driver_Output_Ctrl_cmd),
			Driver_Output_Ctrl_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_LCD_drive_AC_Ctrl_indx),
			config_LCD_drive_AC_Ctrl_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_LCD_drive_AC_Ctrl_cmd),
			config_LCD_drive_AC_Ctrl_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Entry_Mode_indx),
			config_Entry_Mode_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Entry_Mode_cmd),
			config_Entry_Mode_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_1_indx),
			config_Display_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_1_cmd),
			config_Display_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_2_indx),
			config_Display_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_2_cmd),
			config_Display_Ctrl_2_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_3_indx),
			config_Display_Ctrl_3_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_Ctrl_3_cmd),
			config_Display_Ctrl_3_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_1_indx),
			config_Display_IF_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_1_cmd),
			config_Display_IF_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_2_indx),
			config_Display_IF_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Display_IF_Ctrl_2_cmd),
			config_Display_IF_Ctrl_2_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_1_indx),
			config_Panel_IF_Ctrl_1_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_1_cmd),
			config_Panel_IF_Ctrl_1_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_3_indx),
			config_Panel_IF_Ctrl_3_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_3_cmd),
			config_Panel_IF_Ctrl_3_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_4_indx),
			config_Panel_IF_Ctrl_4_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_4_cmd),
			config_Panel_IF_Ctrl_4_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_5_indx),
			config_Panel_IF_Ctrl_5_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_5_cmd),
			config_Panel_IF_Ctrl_5_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_6_indx),
			config_Panel_IF_Ctrl_6_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_6_cmd),
			config_Panel_IF_Ctrl_6_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_8_indx),
			config_Panel_IF_Ctrl_8_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_8_cmd),
			config_Panel_IF_Ctrl_8_cmd },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_9_indx),
			config_Panel_IF_Ctrl_9_indx },
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_9_cmd),
			config_Panel_IF_Ctrl_9_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_00_indx),
			config_gam_adjust_00_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_00_cmd),
			config_gam_adjust_00_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_01_indx),
			config_gam_adjust_01_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_01_cmd),
			config_gam_adjust_01_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_02_indx),
			config_gam_adjust_02_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_02_cmd),
			config_gam_adjust_02_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_03_indx),
			config_gam_adjust_03_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_03_cmd),
			config_gam_adjust_03_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_04_indx), config_gam_adjust_04_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_04_cmd), config_gam_adjust_04_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},


	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_05_indx), config_gam_adjust_05_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_05_cmd), config_gam_adjust_05_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_06_indx), config_gam_adjust_06_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_06_cmd), config_gam_adjust_06_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_07_indx), config_gam_adjust_07_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_07_cmd), config_gam_adjust_07_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_08_indx), config_gam_adjust_08_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_08_cmd), config_gam_adjust_08_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_09_indx), config_gam_adjust_09_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_09_cmd), config_gam_adjust_09_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0A_indx), config_gam_adjust_0A_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0A_cmd), config_gam_adjust_0A_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0B_indx), config_gam_adjust_0B_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0B_cmd), config_gam_adjust_0B_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0C_indx), config_gam_adjust_0C_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0C_cmd), config_gam_adjust_0C_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0D_indx), config_gam_adjust_0D_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_0D_cmd), config_gam_adjust_0D_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_10_indx), config_gam_adjust_10_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_10_cmd), config_gam_adjust_10_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_11_indx), config_gam_adjust_11_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_11_cmd), config_gam_adjust_11_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_12_indx), config_gam_adjust_12_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_12_cmd), config_gam_adjust_12_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_15_indx), config_gam_adjust_15_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_15_cmd), config_gam_adjust_15_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_16_indx), config_gam_adjust_16_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_16_cmd), config_gam_adjust_16_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_17_indx), config_gam_adjust_17_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_17_cmd), config_gam_adjust_17_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_18_indx), config_gam_adjust_18_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_18_cmd), config_gam_adjust_18_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_19_indx), config_gam_adjust_19_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_19_cmd), config_gam_adjust_19_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1C_indx), config_gam_adjust_1C_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1C_cmd), config_gam_adjust_1C_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1D_indx), config_gam_adjust_1D_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_1D_cmd), config_gam_adjust_1D_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_20_indx), config_gam_adjust_20_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_20_cmd), config_gam_adjust_20_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_21_indx), config_gam_adjust_21_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_21_cmd), config_gam_adjust_21_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},


	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_22_indx), config_gam_adjust_22_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_22_cmd), config_gam_adjust_22_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_27_indx), config_gam_adjust_27_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_27_cmd), config_gam_adjust_27_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_28_indx), config_gam_adjust_28_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_28_cmd), config_gam_adjust_28_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_29_indx), config_gam_adjust_29_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_gam_adjust_29_cmd), config_gam_adjust_29_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1_indx), config_Power_Ctrl_1_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_1b_cmd), config_Power_Ctrl_1b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx), config_Power_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2b_cmd), config_Power_Ctrl_2b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_3_indx), config_Power_Ctrl_3_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_3a_cmd), config_Power_Ctrl_3a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_4_indx), config_Power_Ctrl_4_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_4a_cmd), config_Power_Ctrl_4a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_6_indx), config_Power_Ctrl_6_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_6a_cmd), config_Power_Ctrl_6a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_b_cmd),
			config_Auto_Sequencer_Setting_b_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10_indx),
			config_Panel_IF_Ctrl_10_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Panel_IF_Ctrl_10a_cmd),
			config_Panel_IF_Ctrl_10a_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_indx),
			config_Auto_Sequencer_Setting_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Auto_Sequencer_Setting_c_cmd),
			config_Auto_Sequencer_Setting_c_cmd},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},

	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD70), config_DBICADD70},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2_indx),
			config_Power_Ctrl_2_indx},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICSET_15), config_DBICSET_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_DBICADD72), config_DBICADD72},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_Power_Ctrl_2c_cmd),
			config_Power_Ctrl_2c_cmd},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0/* RENESAS_CMD_DELAY */,
		sizeof(config_DBICSET_15), config_DBICSET_15},

};

static char config_WRTXHT2[7] = {0x92, 0x15, 0x05, 0x0F, 0x00, 0x01, 0xe0};
static char config_WRTXVT2[7] = {0x8b, 0x14, 0x01, 0x14, 0x00, 0x03, 0x60};

static struct dsi_cmd_desc renesas_hvga_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXHT2), config_WRTXHT2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_WRTXVT2), config_WRTXVT2},
};

static struct dsi_cmd_desc renesas_video_on_cmds[] = {
{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_VIDEO), config_VIDEO}
};

static struct dsi_cmd_desc renesas_cmd_on_cmds[] = {
{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_CMD_MODE), config_CMD_MODE},
};

extern struct dcs_cmd_req cmdreq;

static char config_mca[2] = {0xB0, 0x04};
static char config_seqctrl[2] = {0xD6, 0x01};
static char config_fmis1_cmd[7] = {0xB3, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00};
static char config_fmis2[3] = {0xB4, 0x0C, 0x00};
static char config_fmis3[3] = {0xB6, 0x39, 0xA3};
static char config_gip[2] = {0xCC, 0x16};
static char config_dispset1common[39] = {
	0xC1, 0x8C, 0x62, 0x40, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x62, 0x30,
	0x40, 0xA5, 0x0F, 0x04,
	0x07, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x01, 0x00};
static char config_dispset2[8] = {0xC2, 0x30, 0xF5, 0x00, 0x0B, 0x0B, 0x00, 0x00};
static char config_srctimming[14] = {0xC4, 0x70, 0x7F, 0x7F, 0x00, 0x80, 0xFF, 0x01, 0x0F, 0x0F, 0x0F, 0x03, 0x01, 0x01};
static char config_giptimming[7] = {0xC6, 0xB6, 0x7F, 0xFF, 0xB6, 0x7F, 0xFF};
static char config_pwmfreq[8] = {0xCE, 0x00, 0x01, 0x88, 0x01, 0x18, 0x00, 0x01};
static char config_pwrsetchargepump[17] = {0xD0, 0x00, 0x11, 0x18, 0x18, 0x98, 0x98, 0x18, 0x01, 0x89, 0x01, 0xFF, 0x4C, 0xC9, 0x0E, 0x21, 0x20};
static char config_pwrsetinternal_24[25] = {0xD3, 0x1B, 0xB3, 0xBB, 0xBB, 0x33, 0x33, 0x33, 0x33, 0x55, 0x01, 0x00, 0xF0, 0xF8, 0xA0, 0x00, 0xC6, 0xB7, 0x33, 0xA2, 0x72, 0xCA, 0x00, 0x00, 0x00};
static char config_vspsensdisable[8] = {0xFE, 0x00, 0x04, 0x0D, 0x00, 0x03, 0x30, 0x06};
static char config_sleepout[2] = {0x11, 0x00};
static char config_sleepin[2] = {0x10, 0x00};

static char config_coladdr[5] = {0x2A, 0x00, 0x00, 0x02, 0xCF};
static char config_pageaddr[5] = {0x2B, 0x00, 0x00, 0x04, 0xFF};
static char config_teon[2] = {0x35, 0x00};
static char config_setaddr[2] = {0x36, 0x00};
static char config_setpixfmt[2] = {0x3A, 0x07};

static char config_dispon[2] = {0x29, 0x00};
static char config_dispoff[2] = {0x28, 0x00};

static char config_wdispbrightness[3] = {0x51, 0x0E, 0xFF};
static char config_wctrldisp[2] = {0x53, 0x2C};
static char config_wcabcctrl[2] = {0x55, 0x02};

static char config_wdispbrightness1[2] = {0x51, 0xFF};
static char config_wctrldisp1[2] = {0x53, 0x2C};
static char config_wcabcctrl1[2] = {0x55, 0x02};

static char config_auo_pre1[2] = {0xFF, 0x05};
static char config_auo_pre2[2] = {0x19, 0x7F};
static char config_auo_pre3[2] = {0xFf, 0x00};

static char config_sleepout1[2] = {0xFF, 0xEE};
static char config_sleepout2[2] = {0xFB, 0x01};
static char config_sleepout21[2] = {0x12, 0x50};
static char config_sleepout22[2] = {0x13, 0x02};
static char config_sleepout23[2] = {0x6A, 0x60};
static char config_sleepout231[2] = {0x04, 0xAD};
static char config_sleepout232[2] = {0xFF, 0x05};
static char config_sleepout233[2] = {0xFB, 0x01};
static char config_sleepout234[2] = {0x19, 0x6F};
static char config_sleepout235[2] = {0xFF, 0x00};
static char config_sleepout24[2] = {0xFF, 0x01};
static char config_sleepout25[2] = {0xFB, 0x01};
static char config_sleepout26[2] = {0x39, 0x01};
static char config_sleepout27[2] = {0xFF, 0x00};
static char config_sleepout28[2] = {0xFB, 0x01};
static char config_sleepout3[2] = {0xBA, 0x03};
static char config_sleepout4[2] = {0xC2, 0x08};

static struct dsi_cmd_desc renesas_display_settings_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_mca), config_mca},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_seqctrl), config_seqctrl},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_fmis1_cmd), config_fmis1_cmd},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_fmis2), config_fmis2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_fmis3), config_fmis3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_gip), config_gip},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_dispset1common), config_dispset1common},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_dispset2), config_dispset2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_srctimming), config_srctimming},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_giptimming), config_giptimming},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_pwmfreq), config_pwmfreq},
};

static struct dsi_cmd_desc renesas_power_settings_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_pwrsetchargepump), config_pwrsetchargepump},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_pwrsetinternal_24), config_pwrsetinternal_24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_vspsensdisable), config_vspsensdisable},
};

static struct dsi_cmd_desc renesas_sleep_out_cmds1[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout1), config_sleepout1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout2), config_sleepout2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout21), config_sleepout21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout22), config_sleepout22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout23), config_sleepout23},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout231), config_sleepout231},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout232), config_sleepout232},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout233), config_sleepout233},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout234), config_sleepout234},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout235), config_sleepout235},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout24), config_sleepout24},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout25), config_sleepout25},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout26), config_sleepout26},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout27), config_sleepout27},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout28), config_sleepout28},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout3), config_sleepout3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_sleepout4), config_sleepout4},
};

static struct dsi_cmd_desc renesas_sleep_out_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_sleepout), config_sleepout},
};

static struct dsi_cmd_desc renesas_user_settings_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_coladdr), config_coladdr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_pageaddr), config_pageaddr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_teon), config_teon},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_setaddr), config_setaddr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_setpixfmt), config_setpixfmt},
};

static struct dsi_cmd_desc renesas_user_settings_cmds1[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_coladdr), config_coladdr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_pageaddr), config_pageaddr},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_teon), config_teon},
};

static struct dsi_cmd_desc renesas_lgd_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_CMD_DELAY,
		sizeof(config_dispon), config_dispon},
};

static struct dsi_cmd_desc renesas_auo_pre_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_auo_pre1), config_auo_pre1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_auo_pre2), config_auo_pre2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_auo_pre3), config_auo_pre3},
};

static struct dsi_cmd_desc renesas_led_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wdispbrightness), config_wdispbrightness},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wctrldisp), config_wctrldisp},
	{DTYPE_DCS_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wcabcctrl), config_wcabcctrl},
};

static struct dsi_cmd_desc renesas_led_on1_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wdispbrightness1), config_wdispbrightness1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wctrldisp1), config_wctrldisp1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, RENESAS_NO_DELAY,
		sizeof(config_wcabcctrl1), config_wcabcctrl1},
};

static struct dsi_cmd_desc renesas_lgd_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_dispoff), config_dispoff},
};

static struct dsi_cmd_desc renesas_lgd_sleep_in_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, RENESAS_SLEEP_OFF_DELAY,
		sizeof(config_sleepin), config_sleepin},
};


static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */
static char manufacture_id0[2] = {0xBF, 0x00}; /* DTYPE_GEN_READ2 */

static struct dsi_cmd_desc renesas_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};

static struct dsi_cmd_desc renesas_manufacture_id0_cmd = {
	DTYPE_GEN_READ2, 1, 0, 1, 5, sizeof(manufacture_id0), manufacture_id0};

static u32 manu_id;
static u32 manu_id0;

static void mipi_renesas_manufature_cb(u32 data)
{
	manu_id = data;
	pr_info("%s: manufature_id=%x\n", __func__, manu_id);
}

static void mipi_renesas_manufature_cb0(u32 data)
{
	manu_id0 = data;
	pr_info("%s: manufature_id0=%x\n", __func__, manu_id0);
}

static uint32 mipi_renesas_manufacture_id(struct msm_fb_data_type *mfd)
{
	cmdreq.cmds = &renesas_manufacture_id_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 3;
	cmdreq.cb = mipi_renesas_manufature_cb;
	mipi_dsi_cmdlist_put(&cmdreq);

	return manu_id;
}

static uint32 mipi_renesas_manufacture_id0(struct msm_fb_data_type *mfd)
{
	cmdreq.cmds = &renesas_manufacture_id0_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 3;
	cmdreq.cb = mipi_renesas_manufature_cb0;
	mipi_dsi_cmdlist_put(&cmdreq);

	return manu_id;
}

extern int mipanel_id(void);
extern void mipanel_set_id(int id);
void sub_mipi_hitachi_renesas_set_dispparam(int param);
void sub_mipi_lgd_renesas_set_dispparam(int param);

static int mipi_lgd_renesas_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	static int blon;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_renesas_manufacture_id(mfd);
	mipi_renesas_manufacture_id0(mfd);

	if ((manu_id == 0 || manu_id == 0x4005 || manu_id == 0x5005) && manu_id0 == 0x13142201) {
		mipanel_set_id(2);
		if (!blon) {
			blon = 1;
			return 0;
		}

		cmdreq.cmds = renesas_display_settings_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_display_settings_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_power_settings_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_power_settings_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_sleep_out_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sleep_out_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_user_settings_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_user_settings_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_lgd_display_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_lgd_display_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_led_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_led_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

	} else if ((manu_id & 0x8000) && manu_id0 == 0) {
		printk("T 2\n");
		mipanel_set_id(3);
		if (!blon) {
			blon = 1;
			return 0;
		}

		cmdreq.cmds = renesas_sleep_out_cmds1;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sleep_out_cmds1);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_sleep_out_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sleep_out_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_auo_pre_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_auo_pre_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_led_on1_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_led_on1_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_user_settings_cmds1;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_user_settings_cmds1);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_lgd_display_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_lgd_display_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

	} else if (manu_id == 0x9999 && manu_id0 == 0x13142201) {
		mipanel_set_id(4);
		if (!blon) {
			blon = 1;
			return 0;
		}

		cmdreq.cmds = renesas_sleep_out_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sleep_out_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_user_settings_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_user_settings_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_lgd_display_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_lgd_display_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

		cmdreq.cmds = renesas_led_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_led_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

	}

	if (dispparam)
		sub_mipi_lgd_renesas_set_dispparam(dispparam);

	return 0;
}

static int mipi_lgd_renesas_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_lgd_display_off_cmds,
			ARRAY_SIZE(renesas_lgd_display_off_cmds));
	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_lgd_sleep_in_cmds,
			ARRAY_SIZE(renesas_lgd_sleep_in_cmds));

#if defined(CONFIG_LEDS_LM3530)
	backlight_brightness_set(0);
#endif
	return 0;
}

static int mipi_renesas_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_sleep_off_cmds,
			ARRAY_SIZE(renesas_sleep_off_cmds));

	mipi_set_tx_power_mode(1);
	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_display_on_cmds,
			ARRAY_SIZE(renesas_display_on_cmds));

	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa() || cpu_is_msm7x25ab()) {
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_hvga_on_cmds,
			ARRAY_SIZE(renesas_hvga_on_cmds));
	}

	if (mipi->mode == DSI_VIDEO_MODE)
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_video_on_cmds,
			ARRAY_SIZE(renesas_video_on_cmds));
	else
		mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_cmd_on_cmds,
			ARRAY_SIZE(renesas_cmd_on_cmds));
	mipi_set_tx_power_mode(0);

	return 0;
}

static int mipi_renesas_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&renesas_tx_buf, renesas_display_off_cmds,
			ARRAY_SIZE(renesas_display_off_cmds));

	return 0;
}

static int mipi_renesas_hitachi_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	static int blon;

	if (!blon) {
		blon = 1;
		return 0;
	}

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

#if defined(CONFIG_LEDS_LM3530)
	backlight_brightness_set(0);
#endif

	if (mipi->mode == DSI_VIDEO_MODE) {
		cmdreq.cmds = renesas_videomode_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_videomode_on_cmds);
	} else if (mipanel_id()) {
		cmdreq.cmds = renesas_hitachi_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_hitachi_on_cmds);
	} else {
		cmdreq.cmds = renesas_sharp_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sharp_on_cmds);
	}
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq);

	if (dispparam || !mipanel_id())
		sub_mipi_hitachi_renesas_set_dispparam(dispparam);

	mipi_renesas_manufacture_id(mfd);

	return 0;
}

static int mipi_renesas_hitachi_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	mipi  = &mfd->panel_info.mipi;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (mipi->mode == DSI_VIDEO_MODE) {
		cmdreq.cmds = renesas_videomode_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_videomode_off_cmds);
	} else if (mipanel_id()) {
		cmdreq.cmds = renesas_hitachi_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_hitachi_off_cmds);
	} else {
		cmdreq.cmds = renesas_sharp_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(renesas_sharp_off_cmds);
	}
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq);

#if defined(CONFIG_LEDS_LM3530)
	backlight_brightness_set(0);
#endif

	return 0;
}

static int __devinit mipi_renesas_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_renesas_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static void mipi_renesas_set_backlight(struct msm_fb_data_type *mfd)
{
	int ret = -EPERM;
	int bl_level;

	bl_level = mfd->bl_level;

#if defined(CONFIG_LEDS_LM3530)
	backlight_brightness_set(bl_level);
	return;
#endif

	if (mipi_renesas_pdata && mipi_renesas_pdata->pmic_backlight)
		ret = mipi_renesas_pdata->pmic_backlight(bl_level);
	else
		pr_err("%s(): Backlight level set failed\n", __func__);
}

void sub_mipi_hitachi_renesas_set_dispparam(int param)
{
}

static void mipi_hitachi_renesas_set_dispparam(int param)
{
	sub_mipi_hitachi_renesas_set_dispparam(param);
}

void sub_mipi_lgd_renesas_set_dispparam(int param)
{
}

static void mipi_lgd_renesas_set_dispparam(int param)
{
	sub_mipi_lgd_renesas_set_dispparam(param);
}

static struct platform_driver this_driver = {
	.probe  = mipi_renesas_lcd_probe,
	.driver = {
		.name   = "mipi_renesas",
	},
};

static struct msm_fb_panel_data renesas_panel_data = {
	.on		= mipi_renesas_lcd_on,
	.off	= mipi_renesas_lcd_off,
	.set_backlight = mipi_renesas_set_backlight,
};

static struct msm_fb_panel_data renesas_hitachi_panel_data = {
	.on		= mipi_renesas_hitachi_on,
	.off	= mipi_renesas_hitachi_off,
	.set_backlight = mipi_renesas_set_backlight,
	.set_dispparam = mipi_hitachi_renesas_set_dispparam,
};

static struct msm_fb_panel_data renesas_lgd_panel_data = {
	.on		= mipi_lgd_renesas_lcd_on,
	.off	= mipi_lgd_renesas_lcd_off,
	.set_backlight = mipi_renesas_set_backlight,
	.set_dispparam = mipi_lgd_renesas_set_dispparam,
};

static int ch_used[3];

int mipi_renesas_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;
	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_renesas_lcd_init();
	if (ret) {
		pr_err("mipi_renesas_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_renesas", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	if (panel == MIPI_DSI_PANEL_720P_PT) {
		if (machine_is_apq8064_mtp()) {
			renesas_hitachi_panel_data.panel_info = *pinfo;
			ret = platform_device_add_data(pdev, &renesas_hitachi_panel_data,
				sizeof(renesas_hitachi_panel_data));
		} else {
			renesas_lgd_panel_data.panel_info = *pinfo;
			ret = platform_device_add_data(pdev, &renesas_lgd_panel_data,
				sizeof(renesas_lgd_panel_data));
		}

	} else {
		renesas_panel_data.panel_info = *pinfo;
		ret = platform_device_add_data(pdev, &renesas_panel_data,
		sizeof(renesas_panel_data));
	}

	if (ret) {
		pr_err("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_renesas_lcd_init(void)
{
	mipi_dsi_buf_alloc(&renesas_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&renesas_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
