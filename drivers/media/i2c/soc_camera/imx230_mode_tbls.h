/*
 * imx230.c - imx230 sensor driver
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <media/camera_common.h>

#ifndef __IMX230_I2C_TABLES__
#define __IMX230_I2C_TABLES__

#define IMX230_TABLE_WAIT_MS	0
#define IMX230_TABLE_END	1
#define IMX230_WAIT_MS		11

#define imx230_reg struct reg_8

static imx230_reg imx230_start[] = {
	{ 0x0100, 0x01 }, /* mode select streaming on */
	{ IMX230_TABLE_END, 0x00 }
};

static imx230_reg imx230_stop[] = {
	{ 0x0100, 0x00 }, /* mode select streaming off */
	{ IMX230_TABLE_END, 0x00 }
};

static imx230_reg tp_colorbars[] = {
	{0x0600, 0x00},
	{0x0601, 0x02},

	{IMX230_TABLE_WAIT_MS, IMX230_WAIT_MS},
	{IMX230_TABLE_END, 0x00}
};

static imx230_reg mode_2672x1600[] = {

	/* Mode setting */
	{0x0114, 0x03},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0222, 0x01},
	{0x0340, 0x06},
	{0x0341, 0x9A},
	{0x0342, 0x17},
	{0x0343, 0x88},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0x98},
	{0x0348, 0x14},
	{0x0349, 0xDF},
	{0x034A, 0x0E},
	{0x034B, 0x17},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x00},
	{0x3000, 0x74},
	{0x3001, 0x00},
	{0x305C, 0x11},

	/* Output size setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x034C, 0x0A},
	{0x034D, 0x70},
	{0x034E, 0x06},
	{0x034F, 0x40},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x0A},
	{0x040D, 0x70},
	{0x040E, 0x06},
	{0x040F, 0x40},

	/* Clock setting */
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0xC8},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0F},
	{0x030E, 0x03},
	{0x030F, 0xA9},
	{0x0310, 0x01},

	/* Data rate setting */
	{0x0820, 0x17},
	{0x0821, 0x6C},
	{0x0822, 0xCC},
	{0x0823, 0xCC},

	/* Integration time setting */
	{0x0202, 0x06},
	{0x0203, 0x90},
	{0x0204, 0x01},
	{0x0225, 0xF4},

	/* Gain setting */
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},

	/* HDR setting */
	{0x3006, 0x01},
	{0x3007, 0x02},
	{0x31E0, 0x03},
	{0x31E1, 0xFF},
	{0x31E4, 0x02},

	/* DPC2D setting */
	{0x3A22, 0x20},
	{0x3A23, 0x14},
	{0x3A24, 0xE0},
	{0x3A25, 0x06},
	{0x3A26, 0x40},
	{0x3A2F, 0x00},
	{0x3A30, 0x00},
	{0x3A31, 0x01},
	{0x3A32, 0x98},
	{0x3A33, 0x14},
	{0x3A34, 0xDF},
	{0x3A35, 0x0E},
	{0x3A36, 0x17},
	{0x3A37, 0x00},
	{0x3A38, 0x01},
	{0x3A39, 0x00},

	/* LSC setting */
	{0x3A21, 0x00},

	/* Stats setting */
	{0x3011, 0x00},
	{0x3013, 0x01},

	{IMX230_TABLE_END, 0x00}
};

static imx230_reg mode_5344x3200[] = {

	/* Mode setting */
	{0x0114, 0x03},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0222, 0x01},
	{0x0340, 0x0C},
	{0x0341, 0xDA},
	{0x0342, 0x17},
	{0x0343, 0x88},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0x98},
	{0x0348, 0x14},
	{0x0349, 0xDF},
	{0x034A, 0x0E},
	{0x034B, 0x17},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x00},
	{0x3000, 0x74},
	{0x3001, 0x00},
	{0x305C, 0x11},

	/* Output size setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x034C, 0x14},
	{0x034D, 0xE0},
	{0x034E, 0x0C},
	{0x034F, 0x80},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x14},
	{0x040D, 0xE0},
	{0x040E, 0x0C},
	{0x040F, 0x80},

	/* Clock setting */
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0xC7},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0F},
	{0x030E, 0x03},
	{0x030F, 0xA9},
	{0x0310, 0x01},

	/* Data rate setting */
	{0x0820, 0x17},
	{0x0821, 0x6C},
	{0x0822, 0xCC},
	{0x0823, 0xCC},

	/* Integration time setting */
	{0x0202, 0x0C},
	{0x0203, 0xD0},
	{0x0204, 0x01},
	{0x0225, 0xF4},

	/* Gain setting */
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},

	/* HDR setting */
	{0x3006, 0x01},
	{0x3007, 0x02},
	{0x31E0, 0x03},
	{0x31E1, 0xFF},
	{0x31E4, 0x02},

	/* DPC2D setting */
	{0x3A22, 0x20},
	{0x3A23, 0x14},
	{0x3A24, 0xE0},
	{0x3A25, 0x0C},
	{0x3A26, 0x80},
	{0x3A2F, 0x00},
	{0x3A30, 0x00},
	{0x3A31, 0x01},
	{0x3A32, 0x98},
	{0x3A33, 0x14},
	{0x3A34, 0xDF},
	{0x3A35, 0x0E},
	{0x3A36, 0x17},
	{0x3A37, 0x00},
	{0x3A38, 0x00},
	{0x3A39, 0x00},

	/* LSC setting */
	{0x3A21, 0x00},

	/* Stats setting */
	{0x3011, 0x00},
	{0x3013, 0x01},

	{IMX230_TABLE_END, 0x00}
};

static imx230_reg mode_5344x4016[] = {

	/* Mode setting */
	{0x0114, 0x03},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0222, 0x01},
	{0x0340, 0x10},
	{0x0341, 0x22},
	{0x0342, 0x17},
	{0x0343, 0x88},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x14},
	{0x0349, 0xDF},
	{0x034A, 0x0F},
	{0x034B, 0xAF},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x00},
	{0x3000, 0x74},
	{0x3001, 0x00},
	{0x305C, 0x11},

	/* Output size setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x034C, 0x14},
	{0x034D, 0xE0},
	{0x034E, 0x0F},
	{0x034F, 0xB0},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x14},
	{0x040D, 0xE0},
	{0x040E, 0x0F},
	{0x040F, 0xB0},

	/* Clock setting */
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0xB1},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0F},
	{0x030E, 0x03},
	{0x030F, 0xA9},
	{0x0310, 0x01},

	/* Data rate setting */
	{0x0820, 0x17},
	{0x0821, 0x6C},
	{0x0822, 0xCC},
	{0x0823, 0xCC},

	/* Integration time setting */
	{0x0202, 0x10},
	{0x0203, 0x18},
	{0x0204, 0x01},
	{0x0225, 0xF4},

	/* Gain setting */
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},

	/* HDR setting */
	{0x3006, 0x01},
	{0x3007, 0x02},
	{0x31E0, 0x03},
	{0x31E1, 0xFF},
	{0x31E4, 0x02},

	/* DPC2D setting */
	{0x3A22, 0x20},
	{0x3A23, 0x14},
	{0x3A24, 0xE0},
	{0x3A25, 0x0F},
	{0x3A26, 0xB0},
	{0x3A2F, 0x00},
	{0x3A30, 0x00},
	{0x3A31, 0x00},
	{0x3A32, 0x00},
	{0x3A33, 0x14},
	{0x3A34, 0xDF},
	{0x3A35, 0x0F},
	{0x3A36, 0xAF},
	{0x3A37, 0x00},
	{0x3A38, 0x00},
	{0x3A39, 0x00},

	/* LSC setting */
	{0x3A21, 0x00},

	/* Stats setting */
	{0x3011, 0x00},
	{0x3013, 0x01},

	{IMX230_TABLE_END, 0x00}

};

static imx230_reg mode_table_common[] = {

	/* external clock setting */
	{0x0136, 0x18},
	{0x0137, 0x00},

	/* global setting */
	{0x4800, 0x0E},
	{0x4890, 0x01},
	{0x4D1E, 0x01},
	{0x4D1F, 0xFF},
	{0x4FA0, 0x00},
	{0x4FA1, 0x00},
	{0x4FA2, 0x00},
	{0x4FA3, 0x83},
	{0x6153, 0x01},
	{0x6156, 0x01},
	{0x69BB, 0x01},
	{0x69BC, 0x05},
	{0x69BD, 0x05},
	{0x69C1, 0x00},
	{0x69C4, 0x01},
	{0x69C6, 0x01},
	{0x7300, 0x00},
	{0x9009, 0x1A},
	{0xB040, 0x90},
	{0xB041, 0x14},
	{0xB042, 0x6B},
	{0xB043, 0x43},
	{0xB044, 0x63},
	{0xB045, 0x2A},
	{0xB046, 0x68},
	{0xB047, 0x06},
	{0xB048, 0x68},
	{0xB049, 0x07},
	{0xB04A, 0x68},
	{0xB04B, 0x04},
	{0xB04C, 0x68},
	{0xB04D, 0x05},
	{0xB04E, 0x68},
	{0xB04F, 0x16},
	{0xB050, 0x68},
	{0xB051, 0x17},
	{0xB052, 0x68},
	{0xB053, 0x74},
	{0xB054, 0x68},
	{0xB055, 0x75},
	{0xB056, 0x68},
	{0xB057, 0x76},
	{0xB058, 0x68},
	{0xB059, 0x77},
	{0xB05A, 0x68},
	{0xB05B, 0x7A},
	{0xB05C, 0x68},
	{0xB05D, 0x7B},
	{0xB05E, 0x68},
	{0xB05F, 0x0A},
	{0xB060, 0x68},
	{0xB061, 0x0B},
	{0xB062, 0x68},
	{0xB063, 0x08},
	{0xB064, 0x68},
	{0xB065, 0x09},
	{0xB066, 0x68},
	{0xB067, 0x0E},
	{0xB068, 0x68},
	{0xB069, 0x0F},
	{0xB06A, 0x68},
	{0xB06B, 0x0C},
	{0xB06C, 0x68},
	{0xB06D, 0x0D},
	{0xB06E, 0x68},
	{0xB06F, 0x13},
	{0xB070, 0x68},
	{0xB071, 0x12},
	{0xB072, 0x90},
	{0xB073, 0x0E},
	{0xD000, 0xDA},
	{0xD001, 0xDA},
	{0xD002, 0x7B},
	{0xD003, 0x00},
	{0xD004, 0x55},
	{0xD005, 0x34},
	{0xD006, 0x21},
	{0xD007, 0x00},
	{0xD008, 0x1C},
	{0xD009, 0x80},
	{0xD00A, 0xFE},
	{0xD00B, 0xC5},
	{0xD00C, 0x55},
	{0xD00D, 0xDC},
	{0xD00E, 0xB6},
	{0xD00F, 0x00},
	{0xD010, 0x31},
	{0xD011, 0x02},
	{0xD012, 0x4A},
	{0xD013, 0x0E},

	/* Load setting */
	{0x5869, 0x01},

	/* Wait at least 10.6ms before stream on */
	{IMX230_TABLE_WAIT_MS, IMX230_WAIT_MS},

	{IMX230_TABLE_END, 0x00}
};

enum {
	IMX230_MODE_2672x1600,
	IMX230_MODE_5344x3200,
	IMX230_MODE_5344x4016,

	IMX230_MODE_COMMON,
	IMX230_MODE_START_STREAM,
	IMX230_MODE_STOP_STREAM,
	IMX230_MODE_TEST_PATTERN,
};

static imx230_reg *mode_table[] = {
	[IMX230_MODE_2672x1600] = mode_2672x1600,
	[IMX230_MODE_5344x3200] = mode_5344x3200,
	[IMX230_MODE_5344x4016] = mode_5344x4016,

	[IMX230_MODE_COMMON] = mode_table_common,
	[IMX230_MODE_START_STREAM] = imx230_start,
	[IMX230_MODE_STOP_STREAM] = imx230_stop,
	[IMX230_MODE_TEST_PATTERN] = tp_colorbars,
};

static const struct camera_common_frmfmt imx230_frmfmt[] = {
	{{2672, 1600},	0, IMX230_MODE_2672x1600},
	{{5344, 3200},	0, IMX230_MODE_5344x3200},
	{{5344, 4016},	0, IMX230_MODE_5344x4016},
};
#endif  /* __IMX230_I2C_TABLES__ */
