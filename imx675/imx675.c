// SPDX-License-Identifier: GPL-2.0
/*
 * Sony IMX675 on the Luckfox Aura CSI1 connector.
 *
 * The register list is the 2608x1960 table that streamed on the Pi 5.
 * 0x3040=0x01 is 2-lane. 0x3014=0x03 is 27 MHz INCK. 0x3023=0x00 is RAW10.
 * Link 720 MHz on this D-PHY. The same table is 800 MHz on the Pi;
 * 1600 Mbps here fails SOT sync on both lanes. Module id 0x96 at
 * 0x3a00. Stream on is 0x3000=0 then
 * 0x3002=0 after the table has settled.
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

#define IMX675_REG_STANDBY		0x3000
#define IMX675_REG_XMSTA		0x3002
#define IMX675_REG_MODULE		0x3a00
#define IMX675_MODULE_ID		0x96

#define IMX675_WIDTH			2608
#define IMX675_HEIGHT			1960
#define IMX675_XCLK_HZ			27000000
#define IMX675_LANES			2

static unsigned int link_mhz = 720;
module_param(link_mhz, uint, 0644);
MODULE_PARM_DESC(link_mhz, "CSI-2 link frequency in MHz (default 800)");

struct imx675_reg {
	u16 addr;
	u8 val;
};

static const struct imx675_reg imx675_2608x1960[] = {
#include "regs.inc"
};

struct imx675 {
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

static const s64 imx675_link_menu[] = {
	445000000, 594000000, 720000000, 800000000,
	891000000, 1188000000,
};

static inline struct imx675 *to_imx675(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx675, sd);
}

static int imx675_write(struct imx675 *sensor, u16 reg, u8 val)
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

static int imx675_read(struct imx675 *sensor, u16 reg, u8 *val)
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

static int imx675_write_table(struct imx675 *sensor)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(imx675_2608x1960); i++) {
		ret = imx675_write(sensor, imx675_2608x1960[i].addr,
				   imx675_2608x1960[i].val);
		if (ret)
			return ret;
	}
	return 0;
}

static int imx675_power(struct imx675 *sensor, bool on)
{
	int ret;

	if (!on) {
		gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
		clk_disable_unprepare(sensor->xclk);
		return 0;
	}

	ret = clk_set_rate(sensor->xclk, IMX675_XCLK_HZ);
	if (ret)
		dev_warn(&sensor->client->dev,
			 "xclk set_rate(%u) failed: %d, rate is %lu\n",
			 IMX675_XCLK_HZ, ret, clk_get_rate(sensor->xclk));
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

static int imx675_identify(struct imx675 *sensor)
{
	u8 id;
	int ret;

	ret = imx675_read(sensor, IMX675_REG_MODULE, &id);
	if (ret)
		return ret;
	if (id != IMX675_MODULE_ID) {
		dev_err(&sensor->client->dev,
			"module id 0x%02x at 0x%04x is not IMX675 (0x%02x)\n",
			id, IMX675_REG_MODULE, IMX675_MODULE_ID);
		return -ENODEV;
	}
	dev_info(&sensor->client->dev,
		 "IMX675 id 0x%02x, %u lanes, link %llu Hz, xclk %u\n",
		 id, IMX675_LANES, sensor->link_freq, IMX675_XCLK_HZ);
	return 0;
}

static int imx675_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx675 *sensor = to_imx675(sd);
	int ret = 0;

	mutex_lock(&sensor->lock);
	if (enable == sensor->streaming)
		goto out;
	if (enable) {
		ret = imx675_write_table(sensor);
		if (!ret) {
			fsleep(10000);
			ret = imx675_write(sensor, IMX675_REG_STANDBY, 0x00);
		}
		if (!ret) {
			fsleep(2000);
			ret = imx675_write(sensor, IMX675_REG_XMSTA, 0x00);
		}
	} else {
		ret = imx675_write(sensor, IMX675_REG_STANDBY, 0x01);
		if (!ret)
			ret = imx675_write(sensor, IMX675_REG_XMSTA, 0x01);
	}
	if (!ret)
		sensor->streaming = enable;
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_mbus_framefmt imx675_fmt = {
	.width = IMX675_WIDTH,
	.height = IMX675_HEIGHT,
	.code = MEDIA_BUS_FMT_SGRBG10_1X10,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_RAW,
	.ycbcr_enc = V4L2_YCBCR_ENC_601,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.xfer_func = V4L2_XFER_FUNC_NONE,
};

static int imx675_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	*v4l2_subdev_get_pad_format(sd, state, 0) = imx675_fmt;
	return 0;
}

static int imx675_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	return 0;
}

static int imx675_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index || fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;
	fse->min_width = fse->max_width = IMX675_WIDTH;
	fse->min_height = fse->max_height = IMX675_HEIGHT;
	return 0;
}

static int imx675_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	fmt->format = *v4l2_subdev_get_pad_format(sd, state, fmt->pad);
	return 0;
}

static int imx675_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	*v4l2_subdev_get_pad_format(sd, state, fmt->pad) = imx675_fmt;
	fmt->format = imx675_fmt;
	return 0;
}

static int imx675_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	if (sel->target != V4L2_SEL_TGT_CROP &&
	    sel->target != V4L2_SEL_TGT_CROP_BOUNDS &&
	    sel->target != V4L2_SEL_TGT_CROP_DEFAULT)
		return -EINVAL;
	sel->r.left = 0;
	sel->r.top = 0;
	sel->r.width = IMX675_WIDTH;
	sel->r.height = IMX675_HEIGHT;
	return 0;
}

static int imx675_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	fi->interval.numerator = 1;
	fi->interval.denominator = 30;
	return 0;
}

static int imx675_get_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				  struct v4l2_mbus_config *config)
{
	struct v4l2_mbus_config_mipi_csi2 *mipi = &config->bus.mipi_csi2;

	if (pad)
		return -EINVAL;
	config->type = V4L2_MBUS_CSI2_DPHY;
	mipi->num_data_lanes = IMX675_LANES;
	return 0;
}

static const struct v4l2_subdev_video_ops imx675_video_ops = {
	.s_stream = imx675_s_stream,
	.g_frame_interval = imx675_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx675_pad_ops = {
	.init_cfg = imx675_init_state,
	.enum_mbus_code = imx675_enum_mbus_code,
	.enum_frame_size = imx675_enum_frame_size,
	.get_fmt = imx675_get_fmt,
	.set_fmt = imx675_set_fmt,
	.get_selection = imx675_get_selection,
	.get_mbus_config = imx675_get_mbus_config,
};

static const struct v4l2_subdev_ops imx675_subdev_ops = {
	.video = &imx675_video_ops,
	.pad = &imx675_pad_ops,
};

static int imx675_init_controls(struct imx675 *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	int ret;

	unsigned int i;
	unsigned int def = 2;

	ret = v4l2_ctrl_handler_init(hdl, 2);
	if (ret)
		return ret;
	/*
	 * 720 MHz is the rate this D-PHY locks. 800 MHz is the Pi
	 * setting and SOT-syncs on both lanes here.
	 */
	for (i = 0; i < ARRAY_SIZE(imx675_link_menu); i++) {
		if (imx675_link_menu[i] == sensor->link_freq)
			def = i;
	}
	v4l2_ctrl_new_int_menu(hdl, NULL, V4L2_CID_LINK_FREQ,
			       ARRAY_SIZE(imx675_link_menu) - 1, def,
			       imx675_link_menu);
	v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_PIXEL_RATE,
			  sensor->pixel_rate, sensor->pixel_rate, 1,
			  sensor->pixel_rate);
	if (hdl->error)
		return hdl->error;
	sensor->sd.ctrl_handler = hdl;
	return 0;
}

static int imx675_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct imx675 *sensor;
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
	sensor->pixel_rate = sensor->link_freq * 2 * IMX675_LANES / 10;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "missing CSI-2 endpoint\n");
	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse endpoint\n");
	if (ep.bus.mipi_csi2.num_data_lanes != IMX675_LANES) {
		v4l2_fwnode_endpoint_free(&ep);
		return dev_err_probe(dev, -EINVAL, "IMX675 is set up as 2-lane\n");
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

	v4l2_i2c_subdev_init(&sensor->sd, client, &imx675_subdev_ops);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;
	ret = imx675_init_controls(sensor);
	if (ret)
		goto err_entity;
	sensor->sd.state_lock = &sensor->lock;
	ret = v4l2_subdev_init_finalize(&sensor->sd);
	if (ret)
		goto err_ctrls;
	ret = imx675_power(sensor, true);
	if (ret)
		goto err_subdev;
	ret = imx675_identify(sensor);
	if (ret)
		goto err_power;
	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
		goto err_power;
	return 0;

err_power:
	imx675_power(sensor, false);
err_subdev:
	v4l2_subdev_cleanup(&sensor->sd);
err_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_entity:
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static void imx675_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx675 *sensor = to_imx675(sd);

	v4l2_async_unregister_subdev(sd);
	imx675_power(sensor, false);
	v4l2_subdev_cleanup(sd);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
	media_entity_cleanup(&sd->entity);
	mutex_destroy(&sensor->lock);
}

static const struct of_device_id imx675_of_match[] = {
	{ .compatible = "sony,imx675" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx675_of_match);

static const struct i2c_device_id imx675_id[] = {
	{ "imx675", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx675_id);

static struct i2c_driver imx675_i2c_driver = {
	.driver = {
		.name = "imx675",
		.of_match_table = imx675_of_match,
	},
	.probe = imx675_probe,
	.remove = imx675_remove,
	.id_table = imx675_id,
};
module_i2c_driver(imx675_i2c_driver);

MODULE_DESCRIPTION("Sony IMX675 CSI-2 sensor driver");
MODULE_LICENSE("GPL");
