// SPDX-License-Identifier: GPL-2.0
/*
 * Sony IMX586 on the Luckfox Aura CSI1 connector.
 *
 * The register list is the 4000x3000 RAW10 table that streamed on the Pi 5.
 * 0x0114=0x03 is 4-lane. 0x0136/0x0137 are 0x1B00, INCK for this 27 MHz
 * crystal. Link 450 MHz (900 Mbps). Chip id 0x0586 at 0x0016.
 * After the table: analog gain 0x0204/0x0205 and 0x0216/0x0217 = 0x03e0,
 * test pattern 0x0601 = 0, then stream on is 0x0100=1.
 * Leave CSI1 IO0 as a GPIO input. Driving it stops the crystal.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stddef.h>

_Static_assert(sizeof(struct i2c_driver) == 248, "i2c_driver size");
_Static_assert(offsetof(struct i2c_driver, probe) == 8, "i2c probe offset");
_Static_assert(offsetof(struct i2c_driver, remove) == 16, "i2c remove offset");
_Static_assert(offsetof(struct i2c_driver, id_table) == 200, "i2c id_table offset");
_Static_assert(sizeof(struct module) == 768, "struct module size");
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

_Static_assert(sizeof(struct v4l2_subdev_pad_ops) == 128, "pad_ops size");
_Static_assert(offsetof(struct v4l2_subdev_pad_ops, get_mbus_config) == 120, "get_mbus_config");
_Static_assert(sizeof(struct v4l2_subdev_video_ops) == 160, "video_ops size");
_Static_assert(offsetof(struct v4l2_subdev_video_ops, s_stream) == 80, "s_stream");
_Static_assert(offsetof(struct v4l2_subdev_video_ops, g_frame_interval) == 96, "g_frame_interval");
_Static_assert(sizeof(struct v4l2_subdev_ops) == 64, "subdev_ops size");
_Static_assert(offsetof(struct v4l2_subdev_ops, video) == 24, "video ops");
_Static_assert(offsetof(struct v4l2_subdev_ops, pad) == 56, "pad ops");

#define IMX586_REG_MODE			0x0100
#define IMX586_REG_CHIP_ID		0x0016
#define IMX586_CHIP_ID			0x0586

#define IMX586_WIDTH			4000
#define IMX586_HEIGHT			3000
#define IMX586_XCLK_HZ			27000000
#define IMX586_LANES			4

static unsigned int link_mhz = 450;
module_param(link_mhz, uint, 0644);
MODULE_PARM_DESC(link_mhz, "CSI-2 link frequency in MHz (default 450)");

struct imx586_reg {
	u16 addr;
	u8 val;
};

static const struct imx586_reg imx586_4000x3000[] = {
#include "regs.inc"
};

struct imx586 {
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct clk *xclk;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct mutex lock;
	bool streaming;
	u64 link_freq;
	u64 pixel_rate;
};

static s64 imx586_link_menu[1];

static inline struct imx586 *to_imx586(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx586, sd);
}

static int imx586_write(struct imx586 *sensor, u16 reg, u8 val)
{
	u8 buf[3] = { reg >> 8, reg & 0xff, val };
	int ret;
	int try;

	for (try = 0; try < 8; try++) {
		ret = i2c_master_send(sensor->client, buf, sizeof(buf));
		if (ret == sizeof(buf))
			return 0;
		fsleep(10000);
	}
	dev_err(&sensor->client->dev, "write 0x%04x failed: %d\n", reg, ret);
	return ret < 0 ? ret : -EIO;
}

static int imx586_read(struct imx586 *sensor, u16 reg, u8 *val)
{
	struct i2c_msg msgs[2];
	u8 addr[2] = { reg >> 8, reg & 0xff };
	int ret;

	msgs[0].addr = sensor->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = addr;
	msgs[1].addr = sensor->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = val;
	ret = i2c_transfer(sensor->client->adapter, msgs, 2);
	if (ret == 2)
		return 0;
	return ret < 0 ? ret : -EIO;
}

static int imx586_write_table(struct imx586 *sensor)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(imx586_4000x3000); i++) {
		ret = imx586_write(sensor, imx586_4000x3000[i].addr,
				   imx586_4000x3000[i].val);
		if (ret)
			return ret;
	}
	return 0;
}

static int imx586_power(struct imx586 *sensor, bool on)
{
	int ret;

	if (!on) {
		gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
		clk_disable_unprepare(sensor->xclk);
		return 0;
	}

	ret = clk_set_rate(sensor->xclk, IMX586_XCLK_HZ);
	if (ret)
		dev_warn(&sensor->client->dev,
			 "xclk set_rate(%u) failed: %d, rate is %lu\n",
			 IMX586_XCLK_HZ, ret, clk_get_rate(sensor->xclk));
	ret = clk_prepare_enable(sensor->xclk);
	if (ret)
		return ret;
	gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
	fsleep(1000);
	gpiod_set_value_cansleep(sensor->reset_gpio, 1);
	fsleep(1000);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 1);
	fsleep(20000);
	if (sensor->reset_gpio)
		dev_info(&sensor->client->dev, "reset gpio %d logical %d\n",
			 desc_to_gpio(sensor->reset_gpio),
			 gpiod_get_value_cansleep(sensor->reset_gpio));
	return 0;
}

static int imx586_identify(struct imx586 *sensor)
{
	u8 hi, lo;
	u16 id;
	int ret;

	ret = imx586_read(sensor, IMX586_REG_CHIP_ID, &hi);
	if (ret)
		return ret;
	ret = imx586_read(sensor, IMX586_REG_CHIP_ID + 1, &lo);
	if (ret)
		return ret;
	id = ((u16)hi << 8) | lo;
	if (id != IMX586_CHIP_ID) {
		dev_err(&sensor->client->dev,
			"chip id 0x%04x is not IMX586 (0x%04x)\n",
			id, IMX586_CHIP_ID);
		return -ENODEV;
	}
	dev_info(&sensor->client->dev,
		 "IMX586 id 0x%04x, %u lanes, link %llu Hz, xclk %u\n",
		 id, IMX586_LANES, sensor->link_freq, IMX586_XCLK_HZ);
	return 0;
}

static int imx586_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx586 *sensor = to_imx586(sd);
	int ret = 0;

	mutex_lock(&sensor->lock);
	if (enable == sensor->streaming)
		goto out;
	if (enable) {
		ret = imx586_write(sensor, IMX586_REG_MODE, 0x00);
		if (!ret)
			ret = imx586_write_table(sensor);
		if (!ret)
			ret = imx586_write(sensor, 0x0204, 0x03);
		if (!ret)
			ret = imx586_write(sensor, 0x0205, 0xe0);
		if (!ret)
			ret = imx586_write(sensor, 0x0216, 0x03);
		if (!ret)
			ret = imx586_write(sensor, 0x0217, 0xe0);
		if (!ret)
			ret = imx586_write(sensor, 0x0601, 0x00);
		if (!ret) {
			fsleep(10000);
			ret = imx586_write(sensor, IMX586_REG_MODE, 0x01);
		}
	} else {
		ret = imx586_write(sensor, IMX586_REG_MODE, 0x00);
	}
	if (!ret)
		sensor->streaming = enable;
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_mbus_framefmt imx586_fmt = {
	.width = IMX586_WIDTH,
	.height = IMX586_HEIGHT,
	.code = MEDIA_BUS_FMT_SRGGB10_1X10,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_RAW,
	.ycbcr_enc = V4L2_YCBCR_ENC_601,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.xfer_func = V4L2_XFER_FUNC_NONE,
};

static int imx586_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	*v4l2_subdev_get_pad_format(sd, state, 0) = imx586_fmt;
	return 0;
}

static int imx586_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	return 0;
}

static int imx586_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index || fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;
	fse->min_width = fse->max_width = IMX586_WIDTH;
	fse->min_height = fse->max_height = IMX586_HEIGHT;
	return 0;
}

static int imx586_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	fmt->format = *v4l2_subdev_get_pad_format(sd, state, fmt->pad);
	return 0;
}

static int imx586_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	*v4l2_subdev_get_pad_format(sd, state, fmt->pad) = imx586_fmt;
	fmt->format = imx586_fmt;
	return 0;
}

static int imx586_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	if (sel->target != V4L2_SEL_TGT_CROP &&
	    sel->target != V4L2_SEL_TGT_CROP_BOUNDS &&
	    sel->target != V4L2_SEL_TGT_CROP_DEFAULT)
		return -EINVAL;
	sel->r.left = 0;
	sel->r.top = 0;
	sel->r.width = IMX586_WIDTH;
	sel->r.height = IMX586_HEIGHT;
	return 0;
}

static int imx586_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	fi->interval.numerator = 1;
	fi->interval.denominator = 10;
	return 0;
}

static int imx586_get_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				  struct v4l2_mbus_config *config)
{
	struct v4l2_mbus_config_mipi_csi2 *mipi = &config->bus.mipi_csi2;

	if (pad)
		return -EINVAL;
	config->type = V4L2_MBUS_CSI2_DPHY;
	mipi->num_data_lanes = IMX586_LANES;
	return 0;
}

static const struct v4l2_subdev_video_ops imx586_video_ops = {
	.s_stream = imx586_s_stream,
	.g_frame_interval = imx586_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx586_pad_ops = {
	.init_cfg = imx586_init_state,
	.enum_mbus_code = imx586_enum_mbus_code,
	.enum_frame_size = imx586_enum_frame_size,
	.get_fmt = imx586_get_fmt,
	.set_fmt = imx586_set_fmt,
	.get_selection = imx586_get_selection,
	.get_mbus_config = imx586_get_mbus_config,
};

static const struct v4l2_subdev_ops imx586_subdev_ops = {
	.video = &imx586_video_ops,
	.pad = &imx586_pad_ops,
};

static int imx586_init_controls(struct imx586 *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	int ret;

	imx586_link_menu[0] = sensor->link_freq;
	ret = v4l2_ctrl_handler_init(hdl, 2);
	if (ret)
		return ret;
	v4l2_ctrl_new_int_menu(hdl, NULL, V4L2_CID_LINK_FREQ, 0, 0,
			       imx586_link_menu);
	v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_PIXEL_RATE,
			  sensor->pixel_rate, sensor->pixel_rate, 1,
			  sensor->pixel_rate);
	if (hdl->error)
		return hdl->error;
	sensor->sd.ctrl_handler = hdl;
	return 0;
}

static int imx586_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct imx586 *sensor;
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	int ret;

	(void)id;
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	sensor->client = client;
	mutex_init(&sensor->lock);
	sensor->link_freq = (u64)link_mhz * 1000000ULL;
	sensor->pixel_rate = sensor->link_freq * 2 * IMX586_LANES / 10;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "missing CSI-2 endpoint\n");
	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse endpoint\n");
	if (ep.bus.mipi_csi2.num_data_lanes != IMX586_LANES) {
		v4l2_fwnode_endpoint_free(&ep);
		return dev_err_probe(dev, -EINVAL, "IMX586 is set up as 4-lane\n");
	}
	v4l2_fwnode_endpoint_free(&ep);

	sensor->xclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sensor->xclk))
		return dev_err_probe(dev, PTR_ERR(sensor->xclk), "xvclk\n");
	sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(sensor->reset_gpio), "reset\n");
	sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->pwdn_gpio))
		return dev_err_probe(dev, PTR_ERR(sensor->pwdn_gpio), "pwdn\n");

	v4l2_i2c_subdev_init(&sensor->sd, client, &imx586_subdev_ops);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;
	ret = imx586_init_controls(sensor);
	if (ret)
		goto err_entity;
	sensor->sd.state_lock = &sensor->lock;
	ret = v4l2_subdev_init_finalize(&sensor->sd);
	if (ret)
		goto err_ctrls;
	ret = imx586_power(sensor, true);
	if (ret)
		goto err_subdev;
	ret = imx586_identify(sensor);
	if (ret)
		goto err_power;
	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
		goto err_power;
	return 0;

err_power:
	imx586_power(sensor, false);
err_subdev:
	v4l2_subdev_cleanup(&sensor->sd);
err_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_entity:
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static void imx586_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx586 *sensor = to_imx586(sd);

	v4l2_async_unregister_subdev(sd);
	imx586_power(sensor, false);
	v4l2_subdev_cleanup(sd);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
	media_entity_cleanup(&sd->entity);
	mutex_destroy(&sensor->lock);
}

static const struct of_device_id imx586_of_match[] = {
	{ .compatible = "camemake,imx586cm" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx586_of_match);

static const struct i2c_device_id imx586_id[] = {
	{ "imx586cm", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx586_id);

static struct i2c_driver imx586_i2c_driver = {
	.driver = {
		.name = "imx586cm",
		.of_match_table = imx586_of_match,
	},
	.probe = imx586_probe,
	.remove = imx586_remove,
	.id_table = imx586_id,
};
module_i2c_driver(imx586_i2c_driver);

MODULE_DESCRIPTION("Sony IMX586 CSI-2 sensor driver");
MODULE_LICENSE("GPL");
