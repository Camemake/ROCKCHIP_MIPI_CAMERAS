// SPDX-License-Identifier: GPL-2.0
/*
 * GalaxyCore GC4023 on the Luckfox Aura CSI1 connector.
 *
 * Linear 2560x1440 table from the Rockchip driver. 0x0114 = 0x01 is 2 lanes.
 * The table is the 27 MHz one. This module's crystal is the sensor clock, so
 * link_mhz defaults to 351. Leave CSI1 IO0 as a GPIO input. Driving it stops
 * the crystal. CSI1 IO1 must be driven high or the chip stays silent.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stddef.h>

/* Running 6.1.141 gc2053.ko: refuse to load a module whose structs do not match. */
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

#define GC4023_REG_CHIP_ID		0x03f0
#define GC4023_CHIP_ID			0x4023
#define GC4023_REG_CTRL_MODE		0x0100
#define GC4023_MODE_STANDBY		0x00
#define GC4023_MODE_STREAMING		0x09
#define GC4023_REG_DELAY		0xfffe

#define GC4023_WIDTH			2560
#define GC4023_HEIGHT			1440
#define GC4023_XCLK_HZ			24000000
#define GC4023_LANES			2

static unsigned int link_mhz = 351;
module_param(link_mhz, uint, 0644);
MODULE_PARM_DESC(link_mhz, "CSI-2 link frequency in MHz (default 351)");

struct gc4023_reg {
	u16 addr;
	u8 val;
};

static const struct gc4023_reg gc4023_init[] = {
	{ 0x03fe, 0xf0 },
	{ 0x03fe, 0x00 },
	{ 0x03fe, 0x10 },
	{ 0x03fe, 0x00 },
	{ 0xfffe, 50 },
	{ 0x0a38, 0x00 },
	{ 0x0a38, 0x01 },
	{ 0x0a20, 0x07 },
	{ 0x061c, 0x50 },
	{ 0x061d, 0x22 },
	{ 0x061e, 0x78 },
	{ 0x061f, 0x06 },
	{ 0x0a21, 0x10 },
	{ 0x0a34, 0x40 },
	{ 0x0a35, 0x01 },
	{ 0x0a36, 0x4e },
	{ 0x0a37, 0x06 },
	{ 0x0314, 0x50 },
	{ 0xfffe, 20 },
	{ 0x0315, 0x00 },
	{ 0x031c, 0xce },
	{ 0x0219, 0x47 },
	{ 0x0342, 0x04 },
	{ 0x0343, 0xb0 },
	{ 0x0259, 0x05 },
	{ 0x025a, 0xa0 },
	{ 0x0340, 0x05 },
	{ 0x0341, 0xdc },
	{ 0x0347, 0x02 },
	{ 0x0348, 0x0a },
	{ 0x0349, 0x08 },
	{ 0x034a, 0x05 },
	{ 0x034b, 0xa8 },
	{ 0x0094, 0x0a },
	{ 0x0095, 0x00 },
	{ 0x0096, 0x05 },
	{ 0x0097, 0xa0 },
	{ 0x0099, 0x04 },
	{ 0x009b, 0x04 },
	{ 0x060c, 0x01 },
	{ 0x060e, 0x08 },
	{ 0x060f, 0x05 },
	{ 0x070c, 0x01 },
	{ 0x070e, 0x08 },
	{ 0x070f, 0x05 },
	{ 0x0909, 0x03 },
	{ 0x0902, 0x04 },
	{ 0x0904, 0x0b },
	{ 0x0907, 0x54 },
	{ 0x0908, 0x06 },
	{ 0x0903, 0x9d },
	{ 0x072a, 0x18 },
	{ 0x0724, 0x0a },
	{ 0x0727, 0x0a },
	{ 0x072a, 0x1c },
	{ 0x072b, 0x0a },
	{ 0x1466, 0x10 },
	{ 0x1468, 0x0b },
	{ 0x1467, 0x13 },
	{ 0x1469, 0x80 },
	{ 0x146a, 0xe8 },
	{ 0x0707, 0x07 },
	{ 0x0737, 0x0f },
	{ 0x0704, 0x01 },
	{ 0x0706, 0x03 },
	{ 0x0716, 0x03 },
	{ 0x0708, 0xc8 },
	{ 0x0718, 0xc8 },
	{ 0x061a, 0x00 },
	{ 0x1430, 0x80 },
	{ 0x1407, 0x10 },
	{ 0x1408, 0x16 },
	{ 0x1409, 0x03 },
	{ 0x146d, 0x0e },
	{ 0x146e, 0x42 },
	{ 0x146f, 0x43 },
	{ 0x1470, 0x3c },
	{ 0x1471, 0x3d },
	{ 0x1472, 0x3a },
	{ 0x1473, 0x3a },
	{ 0x1474, 0x40 },
	{ 0x1475, 0x46 },
	{ 0x1420, 0x14 },
	{ 0x1464, 0x15 },
	{ 0x146c, 0x40 },
	{ 0x146d, 0x40 },
	{ 0x1423, 0x08 },
	{ 0x1428, 0x10 },
	{ 0x1462, 0x18 },
	{ 0x02ce, 0x04 },
	{ 0x143a, 0x0f },
	{ 0x142b, 0x88 },
	{ 0x0245, 0xc9 },
	{ 0x023a, 0x08 },
	{ 0x02cd, 0x99 },
	{ 0x0612, 0x02 },
	{ 0x0613, 0xc7 },
	{ 0x0243, 0x03 },
	{ 0x021b, 0x09 },
	{ 0x0089, 0x03 },
	{ 0x0040, 0xa3 },
	{ 0x0075, 0x64 },
	{ 0x0004, 0x0f },
	{ 0x0002, 0xab },
	{ 0x0053, 0x0a },
	{ 0x0205, 0x0c },
	{ 0x0202, 0x06 },
	{ 0x0203, 0x27 },
	{ 0x0614, 0x00 },
	{ 0x0615, 0x00 },
	{ 0x0181, 0x0c },
	{ 0x0182, 0x05 },
	{ 0x0185, 0x01 },
	{ 0x0180, 0x46 },
	{ 0x0100, 0x08 },
	{ 0x0106, 0x38 },
	{ 0x010d, 0x80 },
	{ 0x010e, 0x0c },
	{ 0x0113, 0x02 },
	{ 0x0114, 0x01 },
	{ 0x0115, 0x10 },
	{ 0x022c, 0x00 },
	{ 0x0a67, 0x80 },
	{ 0x0a54, 0x0e },
	{ 0x0a65, 0x10 },
	{ 0x0a98, 0x10 },
	{ 0x05be, 0x00 },
	{ 0x05a9, 0x01 },
	{ 0x0029, 0x08 },
	{ 0x002b, 0xa8 },
	{ 0x0a83, 0xe0 },
	{ 0x0a72, 0x02 },
	{ 0x0a73, 0x60 },
	{ 0x0a75, 0x41 },
	{ 0x0a70, 0x03 },
	{ 0x0a5a, 0x80 },
	{ 0xfffe, 0x14 },
	{ 0x05be, 0x01 },
	{ 0x0a70, 0x00 },
	{ 0x0080, 0x02 },
	{ 0x0a67, 0x00 },
};

struct gc4023 {
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

static s64 gc4023_link_menu[1];

static inline struct gc4023 *to_gc4023(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc4023, sd);
}

static int gc4023_write(struct gc4023 *sensor, u16 reg, u8 val)
{
	u8 buf[3] = { reg >> 8, reg & 0xff, val };
	int ret = -EIO;
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

static int gc4023_read(struct gc4023 *sensor, u16 reg, u8 *val)
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

static int gc4023_write_table(struct gc4023 *sensor)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(gc4023_init); i++) {
		if (gc4023_init[i].addr == GC4023_REG_DELAY) {
			fsleep(gc4023_init[i].val * 1000);
			continue;
		}
		ret = gc4023_write(sensor, gc4023_init[i].addr,
				   gc4023_init[i].val);
		if (ret)
			return ret;
	}
	return 0;
}

static int gc4023_power(struct gc4023 *sensor, bool on)
{
	int ret;

	if (!on) {
		gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
		clk_disable_unprepare(sensor->xclk);
		return 0;
	}

	ret = clk_set_rate(sensor->xclk, GC4023_XCLK_HZ);
	if (ret)
		dev_warn(&sensor->client->dev,
			 "xclk set_rate(%u) failed: %d, rate is %lu\n",
			 GC4023_XCLK_HZ, ret, clk_get_rate(sensor->xclk));
	ret = clk_prepare_enable(sensor->xclk);
	if (ret)
		return ret;
	gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
	fsleep(20000);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 1);
	fsleep(150);
	gpiod_set_value_cansleep(sensor->reset_gpio, 1);
	fsleep(120000);
	if (sensor->reset_gpio)
		dev_info(&sensor->client->dev, "reset gpio %d logical %d\n",
			 desc_to_gpio(sensor->reset_gpio),
			 gpiod_get_value_cansleep(sensor->reset_gpio));
	return 0;
}

static int gc4023_identify(struct gc4023 *sensor)
{
	u8 hi, lo;
	u16 id;
	int ret;
	int try;

	for (try = 0; try < 5; try++) {
		ret = gc4023_read(sensor, GC4023_REG_CHIP_ID, &hi);
		if (!ret)
			ret = gc4023_read(sensor, GC4023_REG_CHIP_ID + 1, &lo);
		if (!ret)
			break;
		fsleep(20000);
	}
	if (ret) {
		dev_err(&sensor->client->dev, "chip id read failed: %d\n", ret);
		return ret;
	}
	id = ((u16)hi << 8) | lo;
	if (id != GC4023_CHIP_ID) {
		dev_err(&sensor->client->dev,
			"chip id 0x%04x is not GC4023 (0x%04x) at i2c 0x%02x\n",
			id, GC4023_CHIP_ID, sensor->client->addr);
		return -ENODEV;
	}
	dev_info(&sensor->client->dev,
		 "GC4023 id 0x%04x, %u lanes, link %llu Hz, xclk %u\n",
		 id, GC4023_LANES, sensor->link_freq, GC4023_XCLK_HZ);
	return 0;
}

static int gc4023_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc4023 *sensor = to_gc4023(sd);
	int ret = 0;

	mutex_lock(&sensor->lock);
	if (enable == sensor->streaming)
		goto out;
	if (enable) {
		ret = gc4023_write_table(sensor);
		if (!ret)
			ret = gc4023_write(sensor, GC4023_REG_CTRL_MODE,
					   GC4023_MODE_STREAMING);
	} else {
		ret = gc4023_write(sensor, GC4023_REG_CTRL_MODE,
				   GC4023_MODE_STANDBY);
	}
	if (!ret)
		sensor->streaming = enable;
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_mbus_framefmt gc4023_fmt = {
	.width = GC4023_WIDTH,
	.height = GC4023_HEIGHT,
	.code = MEDIA_BUS_FMT_SRGGB10_1X10,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_RAW,
	.ycbcr_enc = V4L2_YCBCR_ENC_601,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.xfer_func = V4L2_XFER_FUNC_NONE,
};

static int gc4023_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	*v4l2_subdev_get_pad_format(sd, state, 0) = gc4023_fmt;
	return 0;
}

static int gc4023_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	return 0;
}

static int gc4023_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index || fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;
	fse->min_width = fse->max_width = GC4023_WIDTH;
	fse->min_height = fse->max_height = GC4023_HEIGHT;
	return 0;
}

static int gc4023_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	fmt->format = *v4l2_subdev_get_pad_format(sd, state, fmt->pad);
	return 0;
}

static int gc4023_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	*v4l2_subdev_get_pad_format(sd, state, fmt->pad) = gc4023_fmt;
	fmt->format = gc4023_fmt;
	return 0;
}

static int gc4023_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	if (sel->target != V4L2_SEL_TGT_CROP &&
	    sel->target != V4L2_SEL_TGT_CROP_BOUNDS &&
	    sel->target != V4L2_SEL_TGT_CROP_DEFAULT)
		return -EINVAL;
	sel->r.left = 0;
	sel->r.top = 0;
	sel->r.width = GC4023_WIDTH;
	sel->r.height = GC4023_HEIGHT;
	return 0;
}

static int gc4023_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	fi->interval.numerator = 1;
	fi->interval.denominator = 30;
	return 0;
}

static int gc4023_get_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				  struct v4l2_mbus_config *config)
{
	struct v4l2_mbus_config_mipi_csi2 *mipi = &config->bus.mipi_csi2;

	if (pad)
		return -EINVAL;
	config->type = V4L2_MBUS_CSI2_DPHY;
	mipi->num_data_lanes = GC4023_LANES;
	return 0;
}

static const struct v4l2_subdev_video_ops gc4023_video_ops = {
	.s_stream = gc4023_s_stream,
	.g_frame_interval = gc4023_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc4023_pad_ops = {
	.init_cfg = gc4023_init_state,
	.enum_mbus_code = gc4023_enum_mbus_code,
	.enum_frame_size = gc4023_enum_frame_size,
	.get_fmt = gc4023_get_fmt,
	.set_fmt = gc4023_set_fmt,
	.get_selection = gc4023_get_selection,
	.get_mbus_config = gc4023_get_mbus_config,
};

static const struct v4l2_subdev_ops gc4023_subdev_ops = {
	.video = &gc4023_video_ops,
	.pad = &gc4023_pad_ops,
};

static int gc4023_init_controls(struct gc4023 *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	int ret;

	gc4023_link_menu[0] = sensor->link_freq;
	ret = v4l2_ctrl_handler_init(hdl, 2);
	if (ret)
		return ret;
	v4l2_ctrl_new_int_menu(hdl, NULL, V4L2_CID_LINK_FREQ, 0, 0,
			       gc4023_link_menu);
	v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_PIXEL_RATE,
			  sensor->pixel_rate, sensor->pixel_rate, 1,
			  sensor->pixel_rate);
	if (hdl->error)
		return hdl->error;
	sensor->sd.ctrl_handler = hdl;
	return 0;
}

static int gc4023_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;

	(void)id;
	struct gc4023 *sensor;
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	int ret;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	sensor->client = client;
	mutex_init(&sensor->lock);
	sensor->link_freq = (u64)link_mhz * 1000000ULL;
	sensor->pixel_rate = sensor->link_freq * 2 * GC4023_LANES / 10;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "missing CSI-2 endpoint\n");
	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse endpoint\n");
	if (ep.bus.mipi_csi2.num_data_lanes != GC4023_LANES) {
		v4l2_fwnode_endpoint_free(&ep);
		return dev_err_probe(dev, -EINVAL, "GC4023 table is 2-lane\n");
	}
	v4l2_fwnode_endpoint_free(&ep);

	sensor->xclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sensor->xclk))
		return dev_err_probe(dev, PTR_ERR(sensor->xclk), "xvclk\n");
	sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(sensor->reset_gpio), "reset\n");
	dev_info(dev, "reset gpio %s\n", sensor->reset_gpio ? "present" : "missing");
	sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->pwdn_gpio))
		return dev_err_probe(dev, PTR_ERR(sensor->pwdn_gpio), "pwdn\n");

	v4l2_i2c_subdev_init(&sensor->sd, client, &gc4023_subdev_ops);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;
	ret = gc4023_init_controls(sensor);
	if (ret)
		goto err_entity;
	sensor->sd.state_lock = &sensor->lock;
	ret = v4l2_subdev_init_finalize(&sensor->sd);
	if (ret)
		goto err_ctrls;
	ret = gc4023_power(sensor, true);
	if (ret)
		goto err_subdev;
	ret = gc4023_identify(sensor);
	if (ret)
		goto err_power;
	ret = gc4023_write_table(sensor);
	if (ret) {
		dev_err(dev, "init table failed: %d\n", ret);
		goto err_power;
	}
	dev_info(dev, "init table written\n");
	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
		goto err_power;
	return 0;

err_power:
	gc4023_power(sensor, false);
err_subdev:
	v4l2_subdev_cleanup(&sensor->sd);
err_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_entity:
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static void gc4023_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc4023 *sensor = to_gc4023(sd);

	v4l2_async_unregister_subdev(sd);
	gc4023_power(sensor, false);
	v4l2_subdev_cleanup(sd);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
	media_entity_cleanup(&sd->entity);
	mutex_destroy(&sensor->lock);
}

static const struct of_device_id gc4023_of_match[] = {
	{ .compatible = "galaxycore,gc4023" },
	{ }
};
MODULE_DEVICE_TABLE(of, gc4023_of_match);

static const struct i2c_device_id gc4023_id[] = {
	{ "gc4023", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc4023_id);

static struct i2c_driver gc4023_i2c_driver = {
	.driver = {
		.name = "gc4023",
		.of_match_table = gc4023_of_match,
	},
	.probe = gc4023_probe,
	.remove = gc4023_remove,
	.id_table = gc4023_id,
};
module_i2c_driver(gc4023_i2c_driver);

MODULE_DESCRIPTION("GalaxyCore GC4023 CSI-2 sensor driver");
MODULE_LICENSE("GPL");
