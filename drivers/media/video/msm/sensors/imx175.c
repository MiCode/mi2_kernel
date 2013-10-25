/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#define SENSOR_NAME "imx175"
#define PLATFORM_DRIVER_NAME "msm_camera_imx175"
#define imx175_obj imx175_##obj

DEFINE_MUTEX(imx175_mut);
static struct msm_sensor_ctrl_t imx175_s_ctrl;

static struct msm_camera_i2c_reg_conf imx175_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx175_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx175_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx175_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx175_snap_settings[] = {
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x87},
	{0x0202, 0x09},
	{0x0203, 0xAD},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x08},
	{0x0348, 0x0C},
	{0x0349, 0xC7},
	{0x034A, 0x09},
	{0x034B, 0x97},
	{0x034C, 0x0C},
	{0x034D, 0xC0},
	{0x034E, 0x09},
	{0x034F, 0x90},
	{0x0390, 0x00},
	{0x3344, 0x4F},
	{0x3345, 0x1F},
	{0x3364, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x67},
	{0x3371, 0x27},
	{0x3372, 0x47},
	{0x3373, 0x27},
	{0x3374, 0x2F},
	{0x3375, 0x27},
	{0x3376, 0x8F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xC0},
	{0x33D6, 0x09},
	{0x33D7, 0x90},
	{0x3302, 0x01},
};

static struct msm_camera_i2c_reg_conf imx175_prev_settings[] = {
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x87},
	{0x0202, 0x09},
	{0x0203, 0xAD},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x08},
	{0x0348, 0x0C},
	{0x0349, 0xC7},
	{0x034A, 0x09},
	{0x034B, 0x97},
	{0x034C, 0x0C},
	{0x034D, 0xC0},
	{0x034E, 0x09},
	{0x034F, 0x90},
	{0x0390, 0x00},
	{0x3344, 0x4F},
	{0x3345, 0x1F},
	{0x3364, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x67},
	{0x3371, 0x27},
	{0x3372, 0x47},
	{0x3373, 0x27},
	{0x3374, 0x2F},
	{0x3375, 0x27},
	{0x3376, 0x8F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xC0},
	{0x33D6, 0x09},
	{0x33D7, 0x90},
	{0x3302, 0x01},
};

static struct msm_camera_i2c_reg_conf imx175_video_60fps_settings[] = {
	{0x030C, 0x00},
	{0x030D, 0x7D},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3344, 0x47},
	{0x3345, 0x1F},
	{0x3370, 0x67},
	{0x3371, 0x1F},
	{0x3372, 0x47},
	{0x3373, 0x27},
	{0x3374, 0x1F},
	{0x3375, 0x1F},
	{0x3376, 0x7F},
	{0x3377, 0x2F},

	{0x0340, 0x03},
	{0x0341, 0xC9},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0x36},
	{0x0348, 0x0C},
	{0x0349, 0xCF},
	{0x034A, 0x08},
	{0x034B, 0x69},
	{0x034C, 0x05},
	{0x034D, 0x20},
	{0x034E, 0x02},
	{0x034F, 0xE0},
	{0x33D4, 0x06},
	{0x33D5, 0x68},
	{0x33D6, 0x03},
	{0x33D7, 0x9A},
	{0x0390, 0x01},
	{0x33C8, 0x00},
	{0x3364, 0x00},
	{0x0401, 0x02},
	{0x0405, 0x14},
	{0x0202, 0x03},
	{0x0203, 0xC5},
};

static struct msm_camera_i2c_reg_conf imx175_video_90fps_settings[] = {
	{0x030C, 0x00},
	{0x030D, 0xA0},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},

	{0x0340, 0x03},
	{0x0341, 0x3B},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x01},
	{0x0345, 0x48},
	{0x0346, 0x01},
	{0x0347, 0xF0},
	{0x0348, 0x0B},
	{0x0349, 0x87},
	{0x034A, 0x07},
	{0x034B, 0xAF},
	{0x034C, 0x05},
	{0x034D, 0x20},
	{0x034E, 0x02},
	{0x034F, 0xE0},
	{0x33D4, 0x05},
	{0x33D5, 0x20},
	{0x33D6, 0x02},
	{0x33D7, 0xE0},
	{0x0390, 0x01},
	{0x33C8, 0x00},
	{0x3364, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x0202, 0x03},
	{0x0203, 0x37},
};

static struct msm_camera_i2c_reg_conf imx175_video_120fps_settings[] = {
	{0x030C, 0x00},
	{0x030D, 0xA0},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},

	{0x0340, 0x02},
	{0x0341, 0x6C},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x28},
	{0x0346, 0x00},
	{0x0347, 0x20},
	{0x0348, 0x0C},
	{0x0349, 0xA7},
	{0x034A, 0x09},
	{0x034B, 0x7F},
	{0x034C, 0x03},
	{0x034D, 0x20},
	{0x034E, 0x02},
	{0x034F, 0x58},
	{0x33D4, 0x03},
	{0x33D5, 0x20},
	{0x33D6, 0x02},
	{0x33D7, 0x58},
	{0x0390, 0x02},
	{0x33C8, 0x00},
	{0x3364, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x0202, 0x02},
	{0x0203, 0x68}
};

static struct msm_camera_i2c_reg_conf imx175_recommend_settings[] = {
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x3020, 0x10},
	{0x302D, 0x02},
	{0x302F, 0x80},
	{0x3032, 0xA3},
	{0x3033, 0x20},
	{0x3034, 0x24},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3050, 0x35},
	{0x3056, 0x57},
	{0x305D, 0x41},
	{0x3097, 0x69},
	{0x3109, 0x41},
	{0x3148, 0x3F},
	{0x330F, 0x07},
	{0x4100, 0x0E},
	{0x4104, 0x32},
	{0x4105, 0x32},
	{0x4108, 0x01},
	{0x4109, 0x7C},
	{0x410A, 0x00},
	{0x410B, 0x00},
};

static struct v4l2_subdev_info imx175_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
};

static struct msm_camera_i2c_conf_array imx175_init_conf[] = {
	{&imx175_recommend_settings[0],
	ARRAY_SIZE(imx175_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx175_confs[] = {
	{&imx175_snap_settings[0],
	ARRAY_SIZE(imx175_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_prev_settings[0],
	ARRAY_SIZE(imx175_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_prev_settings[0],
	ARRAY_SIZE(imx175_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_video_60fps_settings[0],
	ARRAY_SIZE(imx175_video_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_video_90fps_settings[0],
	ARRAY_SIZE(imx175_video_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx175_video_120fps_settings[0],
	ARRAY_SIZE(imx175_video_120fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},

};

static struct msm_sensor_output_info_t imx175_dimensions[] = {
	{
		.x_output = 0xCC0,
		.y_output = 0x990,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x9D0,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0xCC0,
		.y_output = 0x990,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x9D0,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0xCC0,
		.y_output = 0x990,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x9D0,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1312,
		.y_output = 736,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x3C9,
		.vt_pixel_clk = 160000000/*320000000*/,
		.op_pixel_clk = 269000000/*320000000*/,
		.binning_factor = 1,
	},
	{
		.x_output = 1312,
		.y_output = 736,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x33B,
		.vt_pixel_clk = 200000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
	{
		.x_output = 800,
		.y_output = 600,
		.line_length_pclk = 0xD70,
		.frame_length_lines = 0x26C,
		.vt_pixel_clk = 256000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csid_vc_cfg imx175_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx175_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = imx175_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 0x14,
	},
};

static struct msm_camera_csi2_params *imx175_csi_params_array[] = {
	&imx175_csi_params,
	&imx175_csi_params,
	&imx175_csi_params,
	&imx175_csi_params,
	&imx175_csi_params,
	&imx175_csi_params,
};

static struct msm_sensor_output_reg_addr_t imx175_reg_addr = {
	.x_output = 0x034c,
	.y_output = 0x034e,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx175_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0175,
};

static struct msm_sensor_exp_gain_info_t imx175_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x204,
	.vert_offset = 6,
};

static int32_t imx175_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t  offset;
	uint16_t digital_gain_int = 0;
	uint16_t digital_gain = 0x100;

	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;

	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);

	if (gain > 224) {
		digital_gain_int = (gain & 0x00FF) - 224;
		digital_gain = (digital_gain_int << 8) + ((gain & 0xFF00) >> 8);
		gain = 224;
	}
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
				MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x020E, digital_gain, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0210, digital_gain, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0212, digital_gain, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0214, digital_gain, MSM_CAMERA_I2C_WORD_DATA);

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

static char *imx175_name[2] = {"imx175", "imx175l"};

static int32_t imx175_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	uint16_t byte, byte1;

	rc = msm_camera_i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
		MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	if ((chipid != 0x0175) && (chipid == 0x0174)) {
		pr_err("Chip id doesnot match 0x%04x\n", chipid);
		return -ENODEV;
	}

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3382, 0x05, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3383, 0xA0, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3368, 0x18, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3369, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3380, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3400, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	for (rc = 3; rc > 0; rc--) {
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3402, rc, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3410, &byte, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3411, &byte1, MSM_CAMERA_I2C_BYTE_DATA);

		if (byte1 == 0x0C)
			s_ctrl->sensordata->actuator_info->cam_name = 1;

		if (byte == 0x0A) {
			s_ctrl->sensordata->sensor_name = imx175_name[1];
			break;
		} else if (byte == 0x08) {
			s_ctrl->sensordata->sensor_name = imx175_name[0];
			break;
		}
	}

	return rc;
}

static const struct i2c_device_id imx175_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx175_s_ctrl},
	{ }
};

static struct i2c_driver imx175_i2c_driver = {
	.id_table = imx175_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx175_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx175_i2c_driver);
}

static struct v4l2_subdev_core_ops imx175_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx175_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx175_subdev_ops = {
	.core = &imx175_subdev_core_ops,
	.video  = &imx175_subdev_video_ops,
};

static struct msm_sensor_fn_t imx175_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx175_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx175_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = imx175_match_id,
};

static struct msm_sensor_reg_t imx175_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx175_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx175_start_settings),
	.stop_stream_conf = imx175_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx175_stop_settings),
	.group_hold_on_conf = imx175_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx175_groupon_settings),
	.group_hold_off_conf = imx175_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx175_groupoff_settings),
	.init_settings = &imx175_init_conf[0],
	.init_size = ARRAY_SIZE(imx175_init_conf),
	.mode_settings = &imx175_confs[0],
	.output_settings = &imx175_dimensions[0],
	.num_conf = ARRAY_SIZE(imx175_confs),
};

static struct msm_sensor_ctrl_t imx175_s_ctrl = {
	.msm_sensor_reg = &imx175_regs,
	.sensor_i2c_client = &imx175_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &imx175_reg_addr,
	.sensor_id_info = &imx175_id_info,
	.sensor_exp_gain_info = &imx175_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &imx175_csi_params_array[0],
	.msm_sensor_mutex = &imx175_mut,
	.sensor_i2c_driver = &imx175_i2c_driver,
	.sensor_v4l2_subdev_info = imx175_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx175_subdev_info),
	.sensor_v4l2_subdev_ops = &imx175_subdev_ops,
	.func_tbl = &imx175_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 8MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
