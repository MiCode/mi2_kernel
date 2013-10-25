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
#define SENSOR_NAME "imx132"
#define PLATFORM_DRIVER_NAME "msm_camera_imx132"
#define imx132_obj imx132_##obj

DEFINE_MUTEX(imx132_mut);
static struct msm_sensor_ctrl_t imx132_s_ctrl;
static int vendor;

static struct msm_camera_i2c_reg_conf imx132_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx132_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx132_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx132_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx132_snap_settings[] = {
	{0x0340, 0x04},
	{0x0341, 0xA8},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x0C},
	{0x0346, 0x00},
	{0x0347, 0x34},
	{0x0348, 0x07},
	{0x0349, 0xAB},
	{0x034A, 0x04},
	{0x034B, 0x7B},
	{0x034C, 0x07},
	{0x034D, 0xA0},
	{0x034E, 0x04},
	{0x034F, 0x48},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x5A},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30B2, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x18},
	{0x3105, 0x00},
	{0x3106, 0x65},
	{0x3107, 0x00},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3301, 0x01},
	{0x3318, 0x62}
};

static struct msm_camera_i2c_reg_conf imx132_prev_settings[] = {
	{0x0340, 0x04},
	{0x0341, 0xA8},
	{0x0342, 0x08},
	{0x0343, 0xC8},
	{0x0344, 0x00},
	{0x0345, 0x0C},
	{0x0346, 0x00},
	{0x0347, 0x34},
	{0x0348, 0x07},
	{0x0349, 0xAB},
	{0x034A, 0x04},
	{0x034B, 0x7B},
	{0x034C, 0x07},
	{0x034D, 0xA0},
	{0x034E, 0x04},
	{0x034F, 0x48},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x303D, 0x10},
	{0x303E, 0x5A},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x2F},
	{0x304D, 0x02},
	{0x306A, 0x10},
	{0x309B, 0x00},
	{0x309E, 0x41},
	{0x30A0, 0x10},
	{0x30A1, 0x0B},
	{0x30B2, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x0C},
	{0x3103, 0x33},
	{0x3104, 0x18},
	{0x3105, 0x00},
	{0x3106, 0x65},
	{0x3107, 0x00},
	{0x315C, 0x3D},
	{0x315D, 0x3C},
	{0x316E, 0x3E},
	{0x316F, 0x3D},
	{0x3301, 0x01},
	{0x3318, 0x62}
};


static struct msm_camera_i2c_reg_conf imx132_recommend_settings[] = {
	{0x3087, 0x53},
	{0x308B, 0x5A},
	{0x3094, 0x11},
	{0x309D, 0xA4},
	{0x30AA, 0x01},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x3118, 0x2F},
	{0x312A, 0x00},
	{0x312B, 0x0B},
	{0x312C, 0x0B},
	{0x312D, 0x13},
	{0x3032, 0x40},
	{0x0305, 0x02},
	{0x0307, 0x38},
	{0x30A4, 0x02},
	{0x303C, 0x4B}
};

static struct v4l2_subdev_info imx132_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
};

static struct msm_camera_i2c_conf_array imx132_init_conf[] = {
	{&imx132_recommend_settings[0],
	ARRAY_SIZE(imx132_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx132_confs[] = {
	{&imx132_snap_settings[0],
	ARRAY_SIZE(imx132_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx132_prev_settings[0],
	ARRAY_SIZE(imx132_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx132_dimensions[] = {
	{
		.x_output = 1952,
		.y_output = 1096,
		.line_length_pclk = 0x8C8,
		.frame_length_lines = 0x4A8,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1952,
		.y_output = 1096,
		.line_length_pclk = 0x8C8,
		.frame_length_lines = 0x9D0,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 269000000,
		.binning_factor = 1,
	},
};


static struct msm_camera_csid_vc_cfg imx132_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx132_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 1,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = imx132_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,
		.settle_cnt = 0x14,
	},
};

static struct msm_camera_csi2_params *imx132_csi_params_array[] = {
	&imx132_csi_params,
	&imx132_csi_params,
	&imx132_csi_params,
	&imx132_csi_params,
	&imx132_csi_params,
	&imx132_csi_params,
};


static struct msm_sensor_output_reg_addr_t imx132_reg_addr = {
	.x_output = 0x034c,
	.y_output = 0x034e,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx132_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0132,
};

static struct msm_sensor_exp_gain_info_t imx132_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x204,
	.vert_offset = 4,
};

static int32_t imx132_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
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

static int32_t imx132_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = msm_camera_i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
		MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
		s_ctrl->sensordata->sensor_name);
		return rc;
	}

	if (chipid != 0x0132) {
		pr_err("Chip id doesnot match 0x%04x\n", chipid);
		return -ENODEV;
	}
	if (vendor == 0) {
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x351F, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
		if ((chipid & 0x07) == 5) {
			vendor = 2;
			goto init_probe_done;
		} else if ((chipid & 0x07) == 6) {
			vendor = 3 ;
			goto init_probe_done;
		}
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x351E, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
		if ((chipid & 0x70) == 0x50) {
			vendor = 2;
			goto init_probe_done;
		} else if ((chipid & 0x70) == 0x60) {
			vendor = 3;
			goto init_probe_done;
		}
		msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3517, &chipid, MSM_CAMERA_I2C_BYTE_DATA);
		if ((chipid & 0x1C) == 0x14) {
			vendor = 2;
			goto init_probe_done;
		} else if ((chipid & 0x1C) == 0x18) {
			vendor = 3;
			goto init_probe_done;
		}
		vendor = 1;
	}
init_probe_done:
	return rc;
}


static const struct i2c_device_id imx132_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx132_s_ctrl},
	{},
};

static struct i2c_driver imx132_i2c_driver = {
	.id_table = imx132_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx132_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx132_i2c_driver);
}

static struct v4l2_subdev_core_ops imx132_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx132_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx132_subdev_ops = {
	.core = &imx132_subdev_core_ops,
	.video  = &imx132_subdev_video_ops,
};

static struct msm_sensor_fn_t imx132_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx132_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx132_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = imx132_match_id,
};

static struct msm_sensor_reg_t imx132_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx132_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx132_start_settings),
	.stop_stream_conf = imx132_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx132_stop_settings),
	.group_hold_on_conf = imx132_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx132_groupon_settings),
	.group_hold_off_conf = imx132_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx132_groupoff_settings),
	.init_settings = &imx132_init_conf[0],
	.init_size = ARRAY_SIZE(imx132_init_conf),
	.mode_settings = &imx132_confs[0],
	.output_settings = &imx132_dimensions[0],
	.num_conf = ARRAY_SIZE(imx132_confs),
};

static struct msm_sensor_ctrl_t imx132_s_ctrl = {
	.msm_sensor_reg = &imx132_regs,
	.sensor_i2c_client = &imx132_sensor_i2c_client,
	.sensor_i2c_addr = 0x6C,
	.sensor_output_reg_addr = &imx132_reg_addr,
	.sensor_id_info = &imx132_id_info,
	.sensor_exp_gain_info = &imx132_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &imx132_csi_params_array[0],
	.msm_sensor_mutex = &imx132_mut,
	.sensor_i2c_driver = &imx132_i2c_driver,
	.sensor_v4l2_subdev_info = imx132_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx132_subdev_info),
	.sensor_v4l2_subdev_ops = &imx132_subdev_ops,
	.func_tbl = &imx132_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 2MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
