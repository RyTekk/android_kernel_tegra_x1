/*
 * imx214_v4l2.c - imx214 sensor driver
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/v4l2-chip-ident.h>
#include <media/tegra_v4l2_camera.h>
#include <media/camera_common.h>
#include <media/imx214.h>

#include "imx214_mode_tbls.h"

#define IMX214_MAX_COARSE_DIFF		10

#define IMX214_GAIN_SHIFT		8
#define IMX214_MIN_GAIN		(1 << IMX214_GAIN_SHIFT)
#define IMX214_MAX_GAIN		(16 << IMX214_GAIN_SHIFT)
#define IMX214_MIN_FRAME_LENGTH	(0x0)
#define IMX214_MAX_FRAME_LENGTH	(0xffff)
#define IMX214_MIN_EXPOSURE_COARSE	(0x0001)
#define IMX214_MAX_EXPOSURE_COARSE	\
	(IMX214_MAX_FRAME_LENGTH-IMX214_MAX_COARSE_DIFF)

#define IMX214_DEFAULT_GAIN		IMX214_MIN_GAIN
#define IMX214_DEFAULT_FRAME_LENGTH	(0x0C7A)
#define IMX214_DEFAULT_EXPOSURE_COARSE	\
	(IMX214_DEFAULT_FRAME_LENGTH-IMX214_MAX_COARSE_DIFF)

#define IMX214_DEFAULT_MODE	IMX214_MODE_4096X3072
#define IMX214_DEFAULT_HDR_MODE	IMX214_MODE_4096X3072_HDR
#define IMX214_DEFAULT_WIDTH	4096
#define IMX214_DEFAULT_HEIGHT	3072
#define IMX214_DEFAULT_DATAFMT	V4L2_MBUS_FMT_SRGGB10_1X10
#define IMX214_DEFAULT_CLK_FREQ	24000000

#define IMX214_NUM_CROP_REGS	8

struct imx214 {
	struct mutex			imx214_camera_lock;
	struct camera_common_power_rail	power;
	int				numctrls;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct camera_common_eeprom_data eeprom[IMX214_EEPROM_NUM_BLOCKS];
	u8				eeprom_buf[IMX214_EEPROM_SIZE];
	struct i2c_client		*i2c_client;
	struct v4l2_subdev		*subdev;

	s32				group_hold_prev;
	bool				group_hold_en;
	struct regmap			*regmap;
	struct camera_common_data	*s_data;
	struct camera_common_pdata	*pdata;
	struct v4l2_ctrl		*ctrls[];
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int imx214_g_volatile_ctrl(struct v4l2_ctrl *ctrl);
static int imx214_s_ctrl(struct v4l2_ctrl *ctrl);

static const struct v4l2_ctrl_ops imx214_ctrl_ops = {
	.g_volatile_ctrl = imx214_g_volatile_ctrl,
	.s_ctrl		= imx214_s_ctrl,
};

static struct v4l2_ctrl_config ctrl_config_list[] = {
/* Do not change the name field for the controls! */
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX214_MIN_GAIN,
		.max = IMX214_MAX_GAIN,
		.def = IMX214_DEFAULT_GAIN,
		.step = 1,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_FRAME_LENGTH,
		.name = "Frame Length",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX214_MIN_FRAME_LENGTH,
		.max = IMX214_MAX_FRAME_LENGTH,
		.def = IMX214_DEFAULT_FRAME_LENGTH,
		.step = 1,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_COARSE_TIME,
		.name = "Coarse Time",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX214_MIN_EXPOSURE_COARSE,
		.max = IMX214_MAX_EXPOSURE_COARSE,
		.def = IMX214_DEFAULT_EXPOSURE_COARSE,
		.step = 1,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_COARSE_TIME_SHORT,
		.name = "Coarse Time Short",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX214_MIN_EXPOSURE_COARSE,
		.max = IMX214_MAX_EXPOSURE_COARSE,
		.def = IMX214_DEFAULT_EXPOSURE_COARSE,
		.step = 1,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_GROUP_HOLD,
		.name = "Group Hold",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.min = 0,
		.max = ARRAY_SIZE(switch_ctrl_qmenu) - 1,
		.menu_skip_mask = 0,
		.def = 0,
		.qmenu_int = switch_ctrl_qmenu,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_HDR_EN,
		.name = "HDR enable",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.min = 0,
		.max = ARRAY_SIZE(switch_ctrl_qmenu) - 1,
		.menu_skip_mask = 0,
		.def = 0,
		.qmenu_int = switch_ctrl_qmenu,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_EEPROM_DATA,
		.name = "EEPROM Data",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.min = 0,
		.max = IMX214_EEPROM_STR_SIZE,
		.step = 2,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_OTP_DATA,
		.name = "OTP Data",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = IMX214_OTP_STR_SIZE,
		.step = 2,
	},
	{
		.ops = &imx214_ctrl_ops,
		.id = V4L2_CID_FUSE_ID,
		.name = "Fuse ID",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = IMX214_FUSE_ID_STR_SIZE,
		.step = 2,
	},
};

static inline void imx214_get_frame_length_regs(imx214_reg *regs,
				u32 frame_length)
{
	regs->addr = IMX214_FRAME_LENGTH_ADDR_MSB;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX214_FRAME_LENGTH_ADDR_LSB;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void imx214_get_coarse_time_regs(imx214_reg *regs,
				u32 coarse_time)
{
	regs->addr = IMX214_COARSE_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX214_COARSE_TIME_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx214_get_coarse_time_short_regs(imx214_reg *regs,
				u32 coarse_time)
{
	regs->addr = IMX214_COARSE_TIME_SHORT_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX214_COARSE_TIME_SHORT_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx214_get_gain_regs(imx214_reg *regs,
				u16 gain)
{
	regs->addr = IMX214_GAIN_ADDR_MSB;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = IMX214_GAIN_ADDR_LSB;
	(regs + 1)->val = (gain) & 0xff;
}

static inline void imx214_get_gain_short_reg(imx214_reg *regs,
				u16 gain)
{
	regs->addr = IMX214_GAIN_SHORT_ADDR_MSB;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = IMX214_GAIN_SHORT_ADDR_LSB;
	(regs + 1)->val = (gain) & 0xff;
}

static void imx214_get_crop_regs(imx214_reg *regs,
				struct v4l2_rect *rect)
{
	u32 x_start, y_start;
	u32 x_end, y_end;
	x_start = rect->left;
	y_start = rect->top;
	x_end = x_start + rect->width - 1;
	y_end = y_start + rect->height - 1;

	regs->addr = IMX214_CROP_X_START_ADDR_MSB;
	regs->val = (x_start >> 8) & 0xff;
	(regs + 1)->addr = IMX214_CROP_X_START_ADDR_LSB;
	(regs + 1)->val = (x_start) & 0xff;

	(regs + 2)->addr = IMX214_CROP_Y_START_ADDR_MSB;
	(regs + 2)->val = (y_start >> 8) & 0xff;
	(regs + 3)->addr = IMX214_CROP_Y_START_ADDR_LSB;
	(regs + 3)->val = (y_start) & 0xff;

	(regs + 4)->addr = IMX214_CROP_X_END_ADDR_MSB;
	(regs + 4)->val = (x_end >> 8) & 0xff;
	(regs + 5)->addr = IMX214_CROP_X_END_ADDR_LSB;
	(regs + 5)->val = (x_end) & 0xff;

	(regs + 6)->addr = IMX214_CROP_Y_END_ADDR_MSB;
	(regs + 6)->val = (y_end >> 8) & 0xff;
	(regs + 7)->addr = IMX214_CROP_Y_END_ADDR_LSB;
	(regs + 7)->val = (y_end) & 0xff;
}

static int test_mode;
module_param(test_mode, int, 0644);

static inline int imx214_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	struct imx214 *priv = (struct imx214 *)s_data->priv;

	return regmap_read(priv->regmap, addr, (unsigned int *) val);
}

static int imx214_write_reg(struct camera_common_data *s_data, u16 addr, u8 val)
{
	int err;
	struct imx214 *priv = (struct imx214 *)s_data->priv;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int imx214_write_table(struct imx214 *priv,
				const imx214_reg table[])
{
	return regmap_util_write_table_8(priv->regmap,
					 table,
					 NULL, 0,
					 IMX214_TABLE_WAIT_MS,
					 IMX214_TABLE_END);
}

static int imx214_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct imx214 *priv = (struct imx214 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power on\n", __func__);

	if (priv->pdata->power_on) {
		err = priv->pdata->power_on(pw);
		if (err) {
			pr_err("%s failed.\n", __func__);
		} else {
			pw->state = SWITCH_ON;
		}
		return err;
	}

	if (pw->ext_reg1)
		err = regulator_enable(pw->ext_reg1);
	if (unlikely(err))
		goto imx214_ext_reg1_fail;

	if (pw->ext_reg2)
		err = regulator_enable(pw->ext_reg2);
	if (unlikely(err))
		goto imx214_ext_reg2_fail;

	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 0);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 1);
	if (pw->pwdn_gpio)
		gpio_set_value(pw->pwdn_gpio, 0);
	usleep_range(10, 20);

	if (pw->avdd)
		err = regulator_enable(pw->avdd);
	if (err)
		goto imx214_avdd_fail;

	if (pw->iovdd)
		err = regulator_enable(pw->iovdd);
	if (err)
		goto imx214_iovdd_fail;

	usleep_range(1, 2);
	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 1);
	if (pw->pwdn_gpio)
		gpio_set_value(pw->pwdn_gpio, 1);

	usleep_range(300, 310);

	pw->state = SWITCH_ON;
	return 0;

imx214_iovdd_fail:
	regulator_disable(pw->avdd);

imx214_avdd_fail:
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

imx214_ext_reg2_fail:
	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 0);

imx214_ext_reg1_fail:
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int imx214_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct imx214 *priv = (struct imx214 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power off\n", __func__);

	if (priv->pdata->power_off) {
		err = priv->pdata->power_off(pw);
		if (err) {
			pr_err("%s failed.\n", __func__);
			return err;
		} else {
			goto power_off_done;
		}
	}

	usleep_range(1, 2);
	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 0);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 0);
	if (pw->pwdn_gpio)
		gpio_set_value(pw->pwdn_gpio, 0);
	usleep_range(1, 2);

	if (pw->iovdd)
		regulator_disable(pw->iovdd);
	if (pw->avdd)
		regulator_disable(pw->avdd);

	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

power_off_done:
	pw->state = SWITCH_OFF;
	return 0;
}

static int imx214_power_put(struct imx214 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->ext_reg1))
		regulator_put(pw->ext_reg1);

	if (likely(pw->ext_reg2))
		regulator_put(pw->ext_reg2);

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;
	pw->ext_reg1 = NULL;
	pw->ext_reg2 = NULL;

	return 0;
}

static int imx214_get_extra_regulators(struct imx214 *priv,
				struct camera_common_power_rail *pw)
{
	if (!pw->ext_reg1) {
		pw->ext_reg1 = devm_regulator_get(&priv->i2c_client->dev,
						"imx214_reg1");
		if (WARN_ON(IS_ERR(pw->ext_reg1))) {
			pr_err("%s: can't get regulator imx214_reg1: %ld\n",
				__func__, PTR_ERR(pw->ext_reg1));
			pw->ext_reg1 = NULL;
			return -ENODEV;
		}
	}

	if (!pw->ext_reg2) {
		pw->ext_reg2 = devm_regulator_get(&priv->i2c_client->dev,
						"imx214_reg2");
		if (WARN_ON(IS_ERR(pw->ext_reg2))) {
			pr_err("%s: can't get regulator imx214_reg2: %ld\n",
				__func__, PTR_ERR(pw->ext_reg2));
			pw->ext_reg2 = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int imx214_power_get(struct imx214 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;
	struct camera_common_pdata *pdata = priv->pdata;
	const char *mclk_name;
	int err = 0;

	mclk_name = priv->pdata->mclk_name ?
			priv->pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(&priv->i2c_client->dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(&priv->i2c_client->dev,
			"unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	/* ananlog 2.7v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->avdd, pdata->regulators.avdd);
	/* digital 1.2v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->dvdd, pdata->regulators.dvdd);
	/* IO 1.8v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->iovdd, pdata->regulators.iovdd);

	if (pdata->ext_reg)
		err |= imx214_get_extra_regulators(priv, pw);

	if (!err) {
		pw->reset_gpio = pdata->reset_gpio;
		pw->af_gpio = pdata->af_gpio;
		pw->pwdn_gpio = pdata->pwdn_gpio;
	}

	pw->state = SWITCH_OFF;
	return err;
}

static int imx214_set_gain(struct imx214 *priv, s32 val);
static int imx214_set_frame_length(struct imx214 *priv, s32 val);
static int imx214_set_coarse_time(struct imx214 *priv, s32 val);
static int imx214_set_coarse_time_short(struct imx214 *priv, s32 val);
static int imx214_set_crop_data(struct imx214 *priv, struct v4l2_rect *rect);
static int imx214_get_crop_data(struct imx214 *priv, struct v4l2_rect *rect);

static int imx214_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx214 *priv = (struct imx214 *)s_data->priv;
	struct v4l2_control control;
	int err;

	dev_dbg(&client->dev, "%s++\n", __func__);

	if (!enable)
		return imx214_write_table(priv,
			mode_table[IMX214_MODE_STOP_STREAM]);

	err = imx214_write_table(priv, mode_table[IMX214_MODE_COMMON]);
	if (err)
		goto exit;
	err = imx214_write_table(priv, mode_table[s_data->mode]);
	if (err)
		goto exit;

	/* write list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	control.id = V4L2_CID_GAIN;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= imx214_set_gain(priv, control.value);
	if (err)
		dev_dbg(&client->dev, "%s: error gain override\n", __func__);

	control.id = V4L2_CID_FRAME_LENGTH;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= imx214_set_frame_length(priv, control.value);
	if (err)
		dev_dbg(&client->dev,
			"%s: error frame length override\n", __func__);

	control.id = V4L2_CID_COARSE_TIME;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= imx214_set_coarse_time(priv, control.value);
	if (err)
		dev_dbg(&client->dev,
			"%s: error coarse time override\n", __func__);

	control.id = V4L2_CID_COARSE_TIME_SHORT;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= imx214_set_coarse_time_short(priv, control.value);
	if (err)
		dev_dbg(&client->dev,
			"%s: error coarse time short override\n", __func__);

	err = imx214_write_table(priv, mode_table[IMX214_MODE_START_STREAM]);
	if (err)
		goto exit;

	if (test_mode)
		err = imx214_write_table(priv,
			mode_table[IMX214_MODE_TEST_PATTERN]);

	return 0;
exit:
	dev_dbg(&client->dev, "%s: error setting stream\n", __func__);
	return err;
}

static int imx214_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx214 *priv = (struct imx214 *)s_data->priv;
	struct v4l2_rect *rect = &crop->c;
	int err;

	u32 width, height;
	u32 bottom, right;

	width = s_data->fmt_width;
	height = s_data->fmt_height;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if ((width != rect->width) || (height != rect->height)) {
		dev_err(&client->dev,
			"%s: CROP Resolution Mismatch: %dx%d\n",
			__func__, rect->width, rect->height);
		return -EINVAL;
	}
	if (rect->top < 0 || rect->left < 0) {
		dev_err(&client->dev,
			"%s: CROP Bound Error: left:%d, top:%d\n",
			__func__, rect->left, rect->top);
		return -EINVAL;
	}
	right = rect->left + width - 1;
	bottom = rect->top + height - 1;
	if ((right > IMX214_DEFAULT_WIDTH) ||
		(bottom > IMX214_DEFAULT_HEIGHT)) {
		dev_err(&client->dev,
			"%s: CROP Bound Error: right:%d, bottom:%d)\n",
			__func__, right, bottom);
		return -EINVAL;
	}
	err = imx214_set_crop_data(priv, rect);

	return err;
}

static int imx214_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx214 *priv = (struct imx214 *)s_data->priv;
	struct v4l2_rect *rect = &crop->c;
	int err;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	err = imx214_get_crop_data(priv, rect);

	return err;
}

static struct v4l2_subdev_video_ops imx214_subdev_video_ops = {
	.s_stream	= imx214_s_stream,
	.s_crop		= imx214_s_crop,
	.g_crop		= imx214_g_crop,
	.s_mbus_fmt	= camera_common_s_fmt,
	.g_mbus_fmt	= camera_common_g_fmt,
	.try_mbus_fmt	= camera_common_try_fmt,
	.enum_mbus_fmt	= camera_common_enum_fmt,
	.g_mbus_config	= camera_common_g_mbus_config,
};

static struct v4l2_subdev_core_ops imx214_subdev_core_ops = {
	.g_chip_ident	= camera_common_g_chip_ident,
	.s_power	= camera_common_s_power,
};

static struct v4l2_subdev_ops imx214_subdev_ops = {
	.core	= &imx214_subdev_core_ops,
	.video	= &imx214_subdev_video_ops,
};

static struct of_device_id imx214_of_match[] = {
	{ .compatible = "nvidia,imx214", },
	{ },
};

static struct camera_common_sensor_ops imx214_common_ops = {
	.power_on = imx214_power_on,
	.power_off = imx214_power_off,
	.write_reg = imx214_write_reg,
	.read_reg = imx214_read_reg,
};

static int imx214_set_group_hold(struct imx214 *priv)
{
	int err;
	int gh_prev = switch_ctrl_qmenu[priv->group_hold_prev];

	if (priv->group_hold_en == true && gh_prev == SWITCH_OFF) {
		err = imx214_write_reg(priv->s_data,
					   IMX214_GROUP_HOLD_ADDR, 0x1);
		if (err)
			goto fail;
		priv->group_hold_prev = 1;
	} else if (priv->group_hold_en == false && gh_prev == SWITCH_ON) {
		err = imx214_write_reg(priv->s_data,
					   IMX214_GROUP_HOLD_ADDR, 0x0);
		if (err)
			goto fail;
		priv->group_hold_prev = 0;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: Group hold control error\n", __func__);
	return err;
}

int imx214_to_gain(u32 rep, int shift)
{
	int gain;
	int gain_int;
	int gain_dec;
	int min_int = (1 << shift);
	int denom;

	/* last 4 bit of rep is
	 * decimal representation of gain */
	gain_int = (int)(rep >> shift);
	gain_dec = (int)(rep & ~(0xffff << shift));

	denom = gain_int * min_int + gain_dec;
	gain = 512 - ((512 * min_int + (denom - 1)) / denom);

	return gain;
}

static int imx214_set_gain(struct imx214 *priv, s32 val)
{
	imx214_reg reg_list[2];
	imx214_reg reg_list_short[2];
	int err;
	u32 gain;
	int i = 0;

	/* translate value */
	gain = (u16)imx214_to_gain(val, IMX214_GAIN_SHIFT);

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, gain);

	imx214_get_gain_regs(reg_list, gain);
	imx214_get_gain_short_reg(reg_list_short, gain);
	imx214_set_group_hold(priv);

	/* writing long gain */
	for (i = 0; i < 2; i++) {
		err = imx214_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}
	/* writing short gain */
	for (i = 0; i < 2; i++) {
		err = imx214_write_reg(priv->s_data, reg_list_short[i].addr,
			 reg_list_short[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: GAIN control error\n", __func__);
	return err;
}

static int imx214_set_frame_length(struct imx214 *priv, s32 val)
{
	imx214_reg reg_list[2];
	int err;
	u32 frame_length;
	int i = 0;

	frame_length = val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, frame_length);

	imx214_get_frame_length_regs(reg_list, frame_length);
	imx214_set_group_hold(priv);

	for (i = 0; i < 2; i++) {
		err = imx214_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: FRAME_LENGTH control error\n", __func__);
	return err;
}

static int imx214_set_coarse_time(struct imx214 *priv, s32 val)
{
	imx214_reg reg_list[2];
	int err;
	u32 coarse_time;
	int i = 0;

	coarse_time = val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, coarse_time);

	imx214_get_coarse_time_regs(reg_list, coarse_time);
	imx214_set_group_hold(priv);

	for (i = 0; i < 2; i++) {
		err = imx214_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: COARSE_TIME control error\n", __func__);
	return err;
}

static int imx214_set_coarse_time_short(struct imx214 *priv, s32 val)
{
	imx214_reg reg_list[2];
	int err;
	struct v4l2_control hdr_control;
	int hdr_en;
	u32 coarse_time_short;
	int i = 0;

	/* check hdr enable ctrl */
	hdr_control.id = V4L2_CID_HDR_EN;

	err = camera_common_g_ctrl(priv->s_data, &hdr_control);
	if (err < 0) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return err;
	}

	hdr_en = switch_ctrl_qmenu[hdr_control.value];
	if (hdr_en == SWITCH_OFF)
		return 0;

	coarse_time_short = val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, coarse_time_short);

	imx214_get_coarse_time_short_regs(reg_list, coarse_time_short);
	imx214_set_group_hold(priv);

	for (i = 0; i < 2; i++) {
		err  = imx214_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: COARSE_TIME_SHORT control error\n", __func__);
	return err;
}

static int imx214_set_crop_data(struct imx214 *priv, struct v4l2_rect *rect)
{
	imx214_reg reg_list_crop[IMX214_NUM_CROP_REGS];
	int err;
	int i = 0;

	dev_dbg(&priv->i2c_client->dev,
		 "%s:  crop->left:%d, crop->top:%d, crop->res: %dx%d\n",
		 __func__, rect->left, rect->top, rect->width, rect->height);

	imx214_get_crop_regs(reg_list_crop, rect);

	for (i = 0; i < IMX214_NUM_CROP_REGS; i++) {
		err = imx214_write_reg(priv->s_data, reg_list_crop[i].addr,
			 reg_list_crop[i].val);
		if (err) {
			dev_dbg(&priv->i2c_client->dev,
				"%s: SENSOR_CROP control error\n", __func__);
			return err;
		}
	}

	return 0;
}

static int imx214_get_crop_data(struct imx214 *priv, struct v4l2_rect *rect)
{
	imx214_reg reg_list_crop[IMX214_NUM_CROP_REGS];
	int i, err;
	int a, b;
	int right, bottom;

	for (i = 0; i < IMX214_NUM_CROP_REGS; i++) {
		reg_list_crop[i].addr = (IMX214_CROP_X_START_ADDR_MSB + i);
		err = imx214_read_reg(priv->s_data, reg_list_crop[i].addr,
			&reg_list_crop[i].val);
		if (err) {
			dev_dbg(&priv->i2c_client->dev,
				"%s: SENSOR_CROP control error\n", __func__);
			return err;
		}
	}

	a = reg_list_crop[0].val & 0x00ff;
	b = reg_list_crop[1].val & 0x00ff;
	rect->left = (a << 8) | b;

	a = reg_list_crop[2].val & 0x00ff;
	b = reg_list_crop[3].val & 0x00ff;
	rect->top = (a << 8) | b;

	a = reg_list_crop[4].val & 0x00ff;
	b = reg_list_crop[5].val & 0x00ff;
	right = (a << 8) | b;
	rect->width = right - rect->left + 1;

	a = reg_list_crop[6].val & 0x00ff;
	b = reg_list_crop[7].val & 0x00ff;
	bottom = (a << 8) | b;
	rect->height = bottom - rect->top + 1;

	return 0;
}

static int imx214_eeprom_device_release(struct imx214 *priv)
{
	int i;

	for (i = 0; i < IMX214_EEPROM_NUM_BLOCKS; i++) {
		if (priv->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(priv->eeprom[i].i2c_client);
			priv->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static int imx214_eeprom_device_init(struct imx214 *priv)
{
	char *dev_name = "eeprom_imx214";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
	};
	int i;
	int err;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(&priv->ctrl_handler, V4L2_CID_EEPROM_DATA);
	if (!ctrl) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return -EINVAL;
	}

	for (i = 0; i < IMX214_EEPROM_NUM_BLOCKS; i++) {
		priv->eeprom[i].adap = i2c_get_adapter(
				priv->i2c_client->adapter->nr);
		memset(&priv->eeprom[i].brd, 0, sizeof(priv->eeprom[i].brd));
		strncpy(priv->eeprom[i].brd.type, dev_name,
				sizeof(priv->eeprom[i].brd.type));
		priv->eeprom[i].brd.addr = IMX214_EEPROM_ADDRESS + i;
		priv->eeprom[i].i2c_client = i2c_new_device(
				priv->eeprom[i].adap, &priv->eeprom[i].brd);

		priv->eeprom[i].regmap = devm_regmap_init_i2c(
			priv->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(priv->eeprom[i].regmap)) {
			err = PTR_ERR(priv->eeprom[i].regmap);
			imx214_eeprom_device_release(priv);
			ctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return err;
		}
	}

	return 0;
}

static int imx214_read_eeprom(struct imx214 *priv,
				struct v4l2_ctrl *ctrl)
{
	int err, i;

	for (i = 0; i < IMX214_EEPROM_NUM_BLOCKS; i++) {
		err = regmap_bulk_read(priv->eeprom[i].regmap, 0,
			&priv->eeprom_buf[i * IMX214_EEPROM_BLOCK_SIZE],
			IMX214_EEPROM_BLOCK_SIZE);
		if (err)
			return err;
	}

	for (i = 0; i < IMX214_EEPROM_SIZE; i++)
		sprintf(&ctrl->string[i*2], "%02x",
			priv->eeprom_buf[i]);
	return 0;
}

static int imx214_write_eeprom(struct imx214 *priv,
				char *string)
{
	int err;
	int i;
	u8 curr[3];
	unsigned long data;

	for (i = 0; i < IMX214_EEPROM_SIZE; i++) {
		curr[0] = string[i*2];
		curr[1] = string[i*2+1];
		curr[2] = '\0';

		err = kstrtol(curr, 16, &data);
		if (err) {
			dev_err(&priv->i2c_client->dev,
				"invalid eeprom string\n");
			return -EINVAL;
		}

		priv->eeprom_buf[i] = (u8)data;
		err = regmap_write(priv->eeprom[i >> 8].regmap,
				   i & 0xFF, (u8)data);
		if (err)
			return err;
		msleep(20);
	}
	return 0;
}

static int imx214_read_otp_page(struct imx214 *priv,
				u8 *buf, int page, u16 addr, int size)
{
	u8 status;
	int err;

	err = imx214_write_reg(priv->s_data, IMX214_OTP_PAGE_NUM_ADDR, page);
	if (err)
		return err;
	err = imx214_write_reg(priv->s_data, IMX214_OTP_CTRL_ADDR, 0x01);
	if (err)
		return err;
	err = imx214_read_reg(priv->s_data, IMX214_OTP_STATUS_ADDR, &status);
	if (err)
		return err;
	if (status == IMX214_OTP_STATUS_IN_PROGRESS) {
		dev_err(&priv->i2c_client->dev,
			"another OTP read in progress\n");
		return err;
	}

	err = regmap_bulk_read(priv->regmap, addr, buf, size);
	if (err)
		return err;

	err = imx214_read_reg(priv->s_data, IMX214_OTP_STATUS_ADDR, &status);
	if (err)
		return err;
	if (status == IMX214_OTP_STATUS_READ_FAIL) {
		dev_err(&priv->i2c_client->dev, "fuse id read error\n");
		return err;
	}

	return 0;
}

static int imx214_otp_setup(struct imx214 *priv)
{
	int err;
	int i;
	struct v4l2_ctrl *ctrl;
	u8 otp_buf[IMX214_OTP_SIZE];

	err = camera_common_s_power(priv->subdev, true);
	if (err)
		return -ENODEV;

	for (i = 0; i < IMX214_OTP_NUM_PAGES; i++) {
		imx214_read_otp_page(priv,
				   &otp_buf[i * IMX214_OTP_PAGE_SIZE],
				   i,
				   IMX214_OTP_PAGE_START_ADDR,
				   IMX214_OTP_PAGE_SIZE);
	}

	ctrl = v4l2_ctrl_find(&priv->ctrl_handler, V4L2_CID_OTP_DATA);
	if (!ctrl) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return -EINVAL;
	}

	for (i = 0; i < IMX214_OTP_SIZE; i++)
		sprintf(&ctrl->string[i*2], "%02x",
			otp_buf[i]);
	ctrl->cur.string = ctrl->string;

	err = camera_common_s_power(priv->subdev, false);
	if (err)
		return -ENODEV;

	return 0;
}

static int imx214_fuse_id_setup(struct imx214 *priv)
{
	int err;
	int i;
	struct v4l2_ctrl *ctrl;
	u8 fuse_id[IMX214_FUSE_ID_SIZE];

	err = camera_common_s_power(priv->subdev, true);
	if (err)
		return -ENODEV;

	imx214_read_otp_page(priv,
			   &fuse_id[0],
			   IMX214_FUSE_ID_OTP_PAGE,
			   IMX214_FUSE_ID_OTP_ROW_ADDR,
			   IMX214_FUSE_ID_SIZE);

	ctrl = v4l2_ctrl_find(&priv->ctrl_handler, V4L2_CID_FUSE_ID);
	if (!ctrl) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return -EINVAL;
	}

	for (i = 0; i < IMX214_FUSE_ID_SIZE; i++)
		sprintf(&ctrl->string[i*2], "%02x",
			fuse_id[i]);
	ctrl->cur.string = ctrl->string;

	err = camera_common_s_power(priv->subdev, false);
	if (err)
		return -ENODEV;

	return 0;
}

static int imx214_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx214 *priv =
		container_of(ctrl->handler, struct imx214, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EEPROM_DATA:
		err = imx214_read_eeprom(priv, ctrl);
		if (err)
			return err;
		break;
	default:
			pr_err("%s: unknown ctrl id.\n", __func__);
			return -EINVAL;
	}

	return err;
}

static int imx214_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx214 *priv =
		container_of(ctrl->handler, struct imx214, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		err = imx214_set_gain(priv, ctrl->val);
		break;
	case V4L2_CID_FRAME_LENGTH:
		err = imx214_set_frame_length(priv, ctrl->val);
		break;
	case V4L2_CID_COARSE_TIME:
		err = imx214_set_coarse_time(priv, ctrl->val);
		break;
	case V4L2_CID_COARSE_TIME_SHORT:
		err = imx214_set_coarse_time_short(priv, ctrl->val);
		break;
	case V4L2_CID_GROUP_HOLD:
		if (switch_ctrl_qmenu[ctrl->val] == SWITCH_ON) {
			priv->group_hold_en = true;
		} else {
			priv->group_hold_en = false;
			err = imx214_set_group_hold(priv);
		}
		break;
	case V4L2_CID_EEPROM_DATA:
		if (!ctrl->string[0])
			break;
		err = imx214_write_eeprom(priv, ctrl->string);
		if (err)
			return err;
		break;
	case V4L2_CID_HDR_EN:
		break;
	default:
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}

	return err;
}

static int imx214_ctrls_init(struct imx214 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	struct v4l2_ctrl *ctrl;
	int numctrls;
	int err;
	int i;

	dev_dbg(&client->dev, "%s++\n", __func__);

	numctrls = ARRAY_SIZE(ctrl_config_list);
	v4l2_ctrl_handler_init(&priv->ctrl_handler, numctrls);

	for (i = 0; i < numctrls; i++) {
		ctrl = v4l2_ctrl_new_custom(&priv->ctrl_handler,
			&ctrl_config_list[i], NULL);
		if (ctrl == NULL) {
			dev_err(&client->dev, "Failed to init %s ctrl\n",
				ctrl_config_list[i].name);
			continue;
		}

		if (ctrl_config_list[i].type == V4L2_CTRL_TYPE_STRING &&
			ctrl_config_list[i].flags & V4L2_CTRL_FLAG_READ_ONLY) {
			ctrl->string = devm_kzalloc(&client->dev,
				ctrl_config_list[i].max + 1, GFP_KERNEL);
			if (!ctrl->string) {
				dev_err(&client->dev,
					"Failed to allocate otp data\n");
				return -ENOMEM;
			}
		}
		priv->ctrls[i] = ctrl;
	}

	priv->numctrls = numctrls;
	priv->subdev->ctrl_handler = &priv->ctrl_handler;
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls\n",
			priv->ctrl_handler.error);
		err = priv->ctrl_handler.error;
		goto error;
	}

	err = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (err) {
		dev_err(&client->dev,
			"Error %d setting default controls\n", err);
		goto error;
	}

	err = imx214_otp_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"Error %d reading otp data\n", err);
		goto error;
	}

	err = imx214_fuse_id_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"Error %d reading fuse id data\n", err);
		goto error;
	}

	return 0;

error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return err;
}

MODULE_DEVICE_TABLE(of, imx214_of_match);

static struct camera_common_pdata *imx214_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;

	match = of_match_device(imx214_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(&client->dev,
			   sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	board_priv_pdata->pwdn_gpio = of_get_named_gpio(np, "pwdn-gpios", 0);
	board_priv_pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	board_priv_pdata->af_gpio = of_get_named_gpio(np, "af-gpios", 0);

	board_priv_pdata->ext_reg = of_property_read_bool(np, "nvidia,ext_reg");

	of_property_read_string(np, "avdd-reg",
			&board_priv_pdata->regulators.avdd);
	of_property_read_string(np, "dvdd-reg",
			&board_priv_pdata->regulators.dvdd);
	of_property_read_string(np, "iovdd-reg",
			&board_priv_pdata->regulators.iovdd);

	return board_priv_pdata;
}

static int imx214_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct camera_common_data *common_data;
	struct imx214 *priv;
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct tegra_camera_platform_data *imx214_camera_data;
	int err;

	pr_info("[IMX214]: probing v4l2 sensor.\n");

	common_data = devm_kzalloc(&client->dev,
				sizeof(struct camera_common_data), GFP_KERNEL);
	if (!common_data) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv = devm_kzalloc(&client->dev,
			sizeof(struct imx214) + sizeof(struct v4l2_ctrl *) *
			ARRAY_SIZE(ctrl_config_list),
			GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	ssdd = soc_camera_i2c_to_desc(client);
	imx214_camera_data = (struct tegra_camera_platform_data *)
				 ssdd->drv_priv;
	if (!imx214_camera_data) {
		dev_err(&client->dev, "unable to find iclink module name\n");
		return -EFAULT;
	}

	client->dev.of_node = of_find_node_by_name(NULL, "imx214");

	if (client->dev.of_node)
		priv->pdata = imx214_parse_dt(client);
	else {
		priv->pdata = ssdd->dev_priv;
	}

	if (!priv->pdata) {
		dev_err(&client->dev, "unable to get platform data\n");
		return -EFAULT;
	}

	common_data->ops		= &imx214_common_ops;
	common_data->ctrl_handler	= &priv->ctrl_handler;
	common_data->i2c_client		= client;
	common_data->frmfmt		= &imx214_frmfmt[0];
	common_data->colorfmt		= camera_common_find_datafmt(
					  IMX214_DEFAULT_DATAFMT);
	common_data->ctrls		= priv->ctrls;
	common_data->power		= &priv->power;
	common_data->priv		= (void *)priv;
	common_data->ident		= V4L2_IDENT_IMX214;
	common_data->numctrls		= ARRAY_SIZE(ctrl_config_list);
	common_data->numfmts		= ARRAY_SIZE(imx214_frmfmt);
	common_data->def_mode		= IMX214_DEFAULT_MODE;
	common_data->def_width		= IMX214_DEFAULT_WIDTH;
	common_data->def_height		= IMX214_DEFAULT_HEIGHT;
	common_data->def_clk_freq	= IMX214_DEFAULT_CLK_FREQ;
	common_data->csi_port		= (int)imx214_camera_data->port;
	common_data->numlanes		= imx214_camera_data->lanes;

	priv->i2c_client		= client;
	priv->s_data			= common_data;
	priv->subdev			= &common_data->subdev;

	err = imx214_power_get(priv);
	if (err)
		return err;

	camera_common_create_debugfs(common_data, "imx214");

	v4l2_i2c_subdev_init(&common_data->subdev, client, &imx214_subdev_ops);

	err = imx214_ctrls_init(priv);
	if (err)
		return err;

	/* eeprom interface */
	err = imx214_eeprom_device_init(priv);
	if (err)
		dev_err(&client->dev,
			"Failed to allocate eeprom register map: %d\n", err);

	return 0;
}

static int
imx214_remove(struct i2c_client *client)
{
	struct soc_camera_subdev_desc *ssdd;
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx214 *priv = (struct imx214 *)s_data->priv;

	ssdd = soc_camera_i2c_to_desc(client);
	if (ssdd->free_bus)
		ssdd->free_bus(ssdd);

	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	imx214_power_put(priv);
	camera_common_remove_debugfs(s_data);

	return 0;
}

static const struct i2c_device_id imx214_id[] = {
	{ "imx214_v4l2", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx214_id);

static struct i2c_driver imx214_i2c_driver = {
	.driver = {
		.name = "imx214_v4l2",
		.owner = THIS_MODULE,
	},
	.probe = imx214_probe,
	.remove = imx214_remove,
	.id_table = imx214_id,
};

module_i2c_driver(imx214_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for Sony IMX214");
MODULE_AUTHOR("David Wang <davidw@nvidia.com>");
MODULE_LICENSE("GPL v2");

