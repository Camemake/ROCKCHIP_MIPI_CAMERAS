// SPDX-License-Identifier: GPL-2.0
/*
 * Himax HM2170 on the Luckfox Aura CSI1 connector.
 *
 * Same module as the Pi 5 path. Register tables from the Intel IPU6 driver
 * (1928x1088, 2 lanes, GRBG). Those tables were taken at 19.2 MHz. This
 * module's crystal is 27 MHz, so link_mhz defaults to 540 (384 * 27/19.2).
 * Silicon revision picks the table: the third chip-id byte below 4 is rev B,
 * otherwise rev D. Leave CSI1 IO0 as a GPIO input. Driving it stops the crystal.
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

#define HM2170_REG_CHIP_ID		0x0000
#define HM2170_CHIP_ID			0x2170
#define HM2170_REG_MODE			0x0100
#define HM2170_MODE_STANDBY		0x00
#define HM2170_MODE_STREAMING		0x01
#define HM2170_REG_DELAY		0xffff

#define HM2170_WIDTH			1928
#define HM2170_HEIGHT			1088
#define HM2170_XCLK_HZ			27000000
#define HM2170_LANES			2

static unsigned int link_mhz = 540;
module_param(link_mhz, uint, 0644);
MODULE_PARM_DESC(link_mhz, "CSI-2 link frequency in MHz (default 540)");

struct hm2170_reg {
	u16 addr;
	u8 val;
};

static const struct hm2170_reg hm2170_ver_b[] = {
	{ 0x0103, 0x00 },
	{ 0xffff, 0x10 },
	{ 0x0202, 0x03 },
	{ 0x0203, 0x60 },
	{ 0x0300, 0x5e },
	{ 0x0301, 0x3f },
	{ 0x0302, 0x07 },
	{ 0x0303, 0x04 },
	{ 0x0350, 0x61 },
	{ 0x1000, 0xc3 },
	{ 0x1001, 0xc0 },
	{ 0x2000, 0x00 },
	{ 0x2088, 0x01 },
	{ 0x2089, 0x00 },
	{ 0x208a, 0xc8 },
	{ 0x2700, 0x00 },
	{ 0x2711, 0x01 },
	{ 0x2713, 0x04 },
	{ 0x272f, 0x01 },
	{ 0x2800, 0x01 },
	{ 0x2821, 0x8e },
	{ 0x2823, 0x01 },
	{ 0x282e, 0x01 },
	{ 0x282f, 0xc0 },
	{ 0x2839, 0x13 },
	{ 0x283a, 0x01 },
	{ 0x283b, 0x0f },
	{ 0x2842, 0x0c },
	{ 0x2846, 0x01 },
	{ 0x2847, 0x94 },
	{ 0x3001, 0x00 },
	{ 0x3002, 0x88 },
	{ 0x3004, 0x02 },
	{ 0x3024, 0x20 },
	{ 0x3025, 0x12 },
	{ 0x3026, 0x00 },
	{ 0x3027, 0x81 },
	{ 0x3028, 0x01 },
	{ 0x3029, 0x00 },
	{ 0x302a, 0x30 },
	{ 0x3042, 0x00 },
	{ 0x3070, 0x01 },
	{ 0x307b, 0x08 },
	{ 0x30c4, 0x20 },
	{ 0x30d0, 0x01 },
	{ 0x30d2, 0x8e },
	{ 0x30d4, 0x14 },
	{ 0x30d7, 0x21 },
	{ 0x30d9, 0x9e },
	{ 0x30da, 0x14 },
	{ 0x30de, 0x41 },
	{ 0x30e0, 0x9e },
	{ 0x30e2, 0x14 },
	{ 0x30e5, 0x61 },
	{ 0x30e7, 0x9f },
	{ 0x30e9, 0x14 },
	{ 0x30ec, 0x43 },
	{ 0x30ee, 0x9f },
	{ 0x30f0, 0x14 },
	{ 0x30f3, 0x63 },
	{ 0x30f5, 0x9f },
	{ 0x30f6, 0x14 },
	{ 0x30f8, 0x00 },
	{ 0x3101, 0x02 },
	{ 0x3103, 0x9e },
	{ 0x3104, 0x14 },
	{ 0x3108, 0x22 },
	{ 0x310a, 0x9e },
	{ 0x310b, 0x14 },
	{ 0x310f, 0x42 },
	{ 0x3111, 0x9e },
	{ 0x3112, 0x14 },
	{ 0x3116, 0x62 },
	{ 0x3118, 0x9f },
	{ 0x3119, 0x14 },
	{ 0x311d, 0x44 },
	{ 0x311f, 0x9f },
	{ 0x3120, 0x14 },
	{ 0x3124, 0x64 },
	{ 0x3126, 0x9f },
	{ 0x3127, 0x14 },
	{ 0x3135, 0x01 },
	{ 0x3137, 0x03 },
	{ 0x313c, 0x52 },
	{ 0x313e, 0x68 },
	{ 0x3144, 0x3e },
	{ 0x3145, 0x24 },
	{ 0x3146, 0x08 },
	{ 0x3147, 0x13 },
	{ 0x3148, 0x13 },
	{ 0x3149, 0x6c },
	{ 0x314a, 0x13 },
	{ 0x314b, 0x03 },
	{ 0x314c, 0xf8 },
	{ 0x314d, 0x04 },
	{ 0x314e, 0x10 },
	{ 0x3161, 0x11 },
	{ 0x3171, 0x05 },
	{ 0x317a, 0x21 },
	{ 0x317b, 0xf0 },
	{ 0x317c, 0x07 },
	{ 0x317d, 0x09 },
	{ 0x3183, 0x18 },
	{ 0x3184, 0x4a },
	{ 0x318e, 0x88 },
	{ 0x318f, 0x00 },
	{ 0x3190, 0x00 },
	{ 0x4003, 0x02 },
	{ 0x4004, 0x02 },
	{ 0x4800, 0x26 },
	{ 0x4801, 0x10 },
	{ 0x4802, 0x00 },
	{ 0x4803, 0x00 },
	{ 0x4804, 0x3f },
	{ 0x4805, 0x7f },
	{ 0x4806, 0x3f },
	{ 0x4807, 0x1f },
	{ 0x4809, 0x04 },
	{ 0x480a, 0x84 },
	{ 0x480b, 0x08 },
	{ 0x480c, 0x90 },
	{ 0x480d, 0x00 },
	{ 0x480e, 0x00 },
	{ 0x480f, 0x04 },
	{ 0x4810, 0x3f },
	{ 0x4811, 0x00 },
	{ 0x4812, 0x00 },
	{ 0x4813, 0x00 },
	{ 0x4814, 0x00 },
	{ 0x4815, 0x00 },
	{ 0x4816, 0x00 },
	{ 0x4817, 0x00 },
	{ 0x4818, 0x00 },
	{ 0x4819, 0x03 },
	{ 0x481f, 0x00 },
	{ 0x4820, 0x0e },
	{ 0x4821, 0x0e },
	{ 0x4840, 0x00 },
	{ 0x4844, 0x00 },
	{ 0x4845, 0x00 },
	{ 0x4846, 0x00 },
	{ 0x4847, 0x00 },
	{ 0x4848, 0x00 },
	{ 0x4849, 0xf1 },
	{ 0x484a, 0x00 },
	{ 0x484b, 0x88 },
	{ 0x484c, 0x01 },
	{ 0x484d, 0x04 },
	{ 0x484e, 0x64 },
	{ 0x484f, 0x50 },
	{ 0x4850, 0x04 },
	{ 0x4851, 0x00 },
	{ 0x4852, 0x01 },
	{ 0x4853, 0x19 },
	{ 0x4854, 0x50 },
	{ 0x4855, 0x04 },
	{ 0x4856, 0x00 },
	{ 0x4863, 0x02 },
	{ 0x4864, 0x3d },
	{ 0x4865, 0x02 },
	{ 0x4866, 0xb0 },
	{ 0x4880, 0x00 },
	{ 0x48a0, 0x00 },
	{ 0x48a1, 0x04 },
	{ 0x48a2, 0x01 },
	{ 0x48a3, 0xdd },
	{ 0x48a4, 0x0c },
	{ 0x48a5, 0x3b },
	{ 0x48a6, 0x20 },
	{ 0x48a7, 0x20 },
	{ 0x48a8, 0x20 },
	{ 0x48a9, 0x20 },
	{ 0x48aa, 0x00 },
	{ 0x48c0, 0x3f },
	{ 0x48c1, 0x29 },
	{ 0x48c3, 0x14 },
	{ 0x48c4, 0x00 },
	{ 0x48c5, 0x07 },
	{ 0x48c6, 0x88 },
	{ 0x48c7, 0x04 },
	{ 0x48c8, 0x40 },
	{ 0x48c9, 0x00 },
	{ 0x48ca, 0x00 },
	{ 0x48cb, 0x00 },
	{ 0x48cc, 0x1f },
	{ 0x48f0, 0x00 },
	{ 0x48f1, 0x00 },
	{ 0x48f2, 0x04 },
	{ 0x48f3, 0x01 },
	{ 0x48f4, 0xe0 },
	{ 0x48f5, 0x01 },
	{ 0x48f6, 0x10 },
	{ 0x48f7, 0x00 },
	{ 0x48f8, 0x00 },
	{ 0x48f9, 0x00 },
	{ 0x48fa, 0x00 },
	{ 0x48fb, 0x01 },
	{ 0x4931, 0x2b },
	{ 0x4932, 0x01 },
	{ 0x4933, 0x01 },
	{ 0x4934, 0x00 },
	{ 0x4935, 0x0f },
	{ 0x4980, 0x00 },
	{ 0x4a72, 0x01 },
	{ 0x4a73, 0x01 },
	{ 0x4c30, 0x00 },
	{ 0x4cf2, 0x01 },
	{ 0x4cf3, 0x01 },
	{ 0x0104, 0x00 },
};

static const struct hm2170_reg hm2170_ver_d[] = {
	{ 0x0103, 0x00 },
	{ 0xffff, 0x10 },
	{ 0x0202, 0x03 },
	{ 0x0203, 0x60 },
	{ 0x0300, 0x5e },
	{ 0x0301, 0x3f },
	{ 0x0302, 0x07 },
	{ 0x0303, 0x04 },
	{ 0x0350, 0x61 },
	{ 0x1000, 0xc3 },
	{ 0x1001, 0xc0 },
	{ 0x2000, 0x00 },
	{ 0x2088, 0x01 },
	{ 0x2089, 0x00 },
	{ 0x208a, 0xc8 },
	{ 0x2700, 0x00 },
	{ 0x2711, 0x01 },
	{ 0x2713, 0x04 },
	{ 0x272f, 0x01 },
	{ 0x2800, 0x01 },
	{ 0x2821, 0x8e },
	{ 0x2823, 0x01 },
	{ 0x282e, 0x01 },
	{ 0x282f, 0xc0 },
	{ 0x2839, 0x13 },
	{ 0x283a, 0x01 },
	{ 0x283b, 0x0f },
	{ 0x2842, 0x0c },
	{ 0x2846, 0x01 },
	{ 0x2847, 0x94 },
	{ 0x3001, 0x00 },
	{ 0x3002, 0x88 },
	{ 0x3004, 0x02 },
	{ 0x3024, 0x20 },
	{ 0x3025, 0x12 },
	{ 0x3026, 0x00 },
	{ 0x3027, 0x81 },
	{ 0x3028, 0x01 },
	{ 0x3029, 0x00 },
	{ 0x302a, 0x30 },
	{ 0x3042, 0x00 },
	{ 0x3070, 0x01 },
	{ 0x307b, 0x08 },
	{ 0x30c4, 0x20 },
	{ 0x30d0, 0x02 },
	{ 0x30d1, 0x03 },
	{ 0x30d2, 0x3f },
	{ 0x30d3, 0x15 },
	{ 0x30d7, 0x03 },
	{ 0x30d8, 0x03 },
	{ 0x30d9, 0x3f },
	{ 0x30da, 0x15 },
	{ 0x30de, 0x04 },
	{ 0x30df, 0x03 },
	{ 0x30e0, 0x3f },
	{ 0x30e1, 0x15 },
	{ 0x30e5, 0x24 },
	{ 0x30e6, 0x03 },
	{ 0x30e7, 0x3f },
	{ 0x30e8, 0x15 },
	{ 0x30ec, 0x2c },
	{ 0x30ed, 0x03 },
	{ 0x30ee, 0x3f },
	{ 0x30ef, 0x15 },
	{ 0x30f3, 0x2c },
	{ 0x30f4, 0x03 },
	{ 0x30f5, 0x3f },
	{ 0x30f6, 0x15 },
	{ 0x30f8, 0x01 },
	{ 0x3101, 0x02 },
	{ 0x3102, 0x01 },
	{ 0x3103, 0x1f },
	{ 0x3104, 0x15 },
	{ 0x3108, 0x03 },
	{ 0x3109, 0x01 },
	{ 0x310a, 0x1f },
	{ 0x310b, 0x14 },
	{ 0x310f, 0x04 },
	{ 0x3110, 0x01 },
	{ 0x3111, 0x1f },
	{ 0x3112, 0x13 },
	{ 0x3116, 0x24 },
	{ 0x3117, 0x01 },
	{ 0x3118, 0x3f },
	{ 0x3119, 0x13 },
	{ 0x311d, 0x2c },
	{ 0x311e, 0x01 },
	{ 0x311f, 0x3f },
	{ 0x3120, 0x13 },
	{ 0x3121, 0x94 },
	{ 0x3124, 0x2c },
	{ 0x3125, 0x01 },
	{ 0x3126, 0x3f },
	{ 0x3127, 0x13 },
	{ 0x3129, 0x01 },
	{ 0x3135, 0x01 },
	{ 0x3137, 0x03 },
	{ 0x3139, 0x37 },
	{ 0x313c, 0x52 },
	{ 0x313e, 0x68 },
	{ 0x3144, 0x3e },
	{ 0x3145, 0xe4 },
	{ 0x3146, 0x58 },
	{ 0x3147, 0x13 },
	{ 0x3148, 0x11 },
	{ 0x3149, 0x27 },
	{ 0x314a, 0x13 },
	{ 0x314b, 0x03 },
	{ 0x314c, 0x0c },
	{ 0x314d, 0x00 },
	{ 0x314e, 0x10 },
	{ 0x3158, 0x01 },
	{ 0x3161, 0x11 },
	{ 0x3171, 0x05 },
	{ 0x317a, 0x21 },
	{ 0x317b, 0xf0 },
	{ 0x317c, 0x0c },
	{ 0x317d, 0x09 },
	{ 0x3182, 0x88 },
	{ 0x3183, 0x18 },
	{ 0x3184, 0x40 },
	{ 0x318e, 0x88 },
	{ 0x318f, 0x00 },
	{ 0x3190, 0x00 },
	{ 0x4003, 0x02 },
	{ 0x4004, 0x02 },
	{ 0x4800, 0x26 },
	{ 0x4801, 0x21 },
	{ 0x4802, 0x10 },
	{ 0x4803, 0x00 },
	{ 0x4804, 0x3f },
	{ 0x4805, 0x7f },
	{ 0x4806, 0x3f },
	{ 0x4807, 0x1f },
	{ 0x4809, 0x04 },
	{ 0x480a, 0x84 },
	{ 0x480b, 0x04 },
	{ 0x480c, 0x48 },
	{ 0x480d, 0x00 },
	{ 0x480e, 0x00 },
	{ 0x480f, 0x04 },
	{ 0x4810, 0x3f },
	{ 0x4811, 0x00 },
	{ 0x4812, 0x00 },
	{ 0x4813, 0x00 },
	{ 0x4814, 0x00 },
	{ 0x4815, 0x00 },
	{ 0x4816, 0x00 },
	{ 0x4817, 0x00 },
	{ 0x4818, 0x00 },
	{ 0x4819, 0x02 },
	{ 0x481f, 0x00 },
	{ 0x4820, 0x0e },
	{ 0x4821, 0x0e },
	{ 0x4840, 0x00 },
	{ 0x4844, 0x00 },
	{ 0x4845, 0x00 },
	{ 0x4846, 0x00 },
	{ 0x4847, 0x00 },
	{ 0x4848, 0x00 },
	{ 0x4849, 0xf1 },
	{ 0x484a, 0x00 },
	{ 0x484b, 0x88 },
	{ 0x484c, 0x01 },
	{ 0x484d, 0x04 },
	{ 0x484e, 0x64 },
	{ 0x484f, 0x50 },
	{ 0x4850, 0x04 },
	{ 0x4851, 0x00 },
	{ 0x4852, 0x01 },
	{ 0x4853, 0x19 },
	{ 0x4854, 0x50 },
	{ 0x4855, 0x04 },
	{ 0x4856, 0x00 },
	{ 0x4863, 0x02 },
	{ 0x4864, 0x3d },
	{ 0x4865, 0x02 },
	{ 0x4866, 0xb0 },
	{ 0x4880, 0x00 },
	{ 0x48a0, 0x00 },
	{ 0x48a1, 0x04 },
	{ 0x48a2, 0x01 },
	{ 0x48a3, 0xdd },
	{ 0x48a4, 0x0c },
	{ 0x48a5, 0x3b },
	{ 0x48a6, 0x20 },
	{ 0x48a7, 0x20 },
	{ 0x48a8, 0x20 },
	{ 0x48a9, 0x20 },
	{ 0x48aa, 0x00 },
	{ 0x48c0, 0x3f },
	{ 0x48c1, 0x29 },
	{ 0x48c3, 0x14 },
	{ 0x48c4, 0x00 },
	{ 0x48c5, 0x07 },
	{ 0x48c6, 0x88 },
	{ 0x48c7, 0x04 },
	{ 0x48c8, 0x40 },
	{ 0x48c9, 0x00 },
	{ 0x48ca, 0x00 },
	{ 0x48cb, 0x00 },
	{ 0x48cc, 0x1f },
	{ 0x48f0, 0x00 },
	{ 0x48f1, 0x00 },
	{ 0x48f2, 0x04 },
	{ 0x48f3, 0x01 },
	{ 0x48f4, 0xe0 },
	{ 0x48f5, 0x01 },
	{ 0x48f6, 0x10 },
	{ 0x48f7, 0x00 },
	{ 0x48f8, 0x00 },
	{ 0x48f9, 0x00 },
	{ 0x48fa, 0x00 },
	{ 0x48fb, 0x01 },
	{ 0x4931, 0x2b },
	{ 0x4932, 0x01 },
	{ 0x4933, 0x01 },
	{ 0x4934, 0x00 },
	{ 0x4935, 0x0f },
	{ 0x4980, 0x00 },
	{ 0x4a72, 0x01 },
	{ 0x4a73, 0x01 },
	{ 0x4c30, 0x00 },
	{ 0x4cf2, 0x01 },
	{ 0x4cf3, 0x01 },
	{ 0x0104, 0x00 },
};

struct hm2170 {
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
	const struct hm2170_reg *table;
	unsigned int nregs;
	u8 rev;
};

static s64 hm2170_link_menu[1];

static inline struct hm2170 *to_hm2170(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hm2170, sd);
}

static int hm2170_write(struct hm2170 *sensor, u16 reg, u8 val)
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

static int hm2170_read(struct hm2170 *sensor, u16 reg, u8 *buf, int len)
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
	msgs[1].len = len;
	msgs[1].buf = buf;
	ret = i2c_transfer(sensor->client->adapter, msgs, 2);
	if (ret == 2)
		return 0;
	return ret < 0 ? ret : -EIO;
}

static int hm2170_write_table(struct hm2170 *sensor)
{
	unsigned int i;
	int ret;

	for (i = 0; i < sensor->nregs; i++) {
		if (sensor->table[i].addr == HM2170_REG_DELAY) {
			fsleep(sensor->table[i].val * 1000);
			continue;
		}
		ret = hm2170_write(sensor, sensor->table[i].addr,
				   sensor->table[i].val);
		if (ret)
			return ret;
	}
	return 0;
}

static int hm2170_power(struct hm2170 *sensor, bool on)
{
	int ret;

	if (!on) {
		gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
		clk_disable_unprepare(sensor->xclk);
		return 0;
	}

	ret = clk_set_rate(sensor->xclk, HM2170_XCLK_HZ);
	if (ret)
		dev_warn(&sensor->client->dev,
			 "xclk set_rate(%u) failed: %d, rate is %lu\n",
			 HM2170_XCLK_HZ, ret, clk_get_rate(sensor->xclk));
	ret = clk_prepare_enable(sensor->xclk);
	if (ret)
		return ret;
	gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
	fsleep(20000);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 1);
	fsleep(150);
	gpiod_set_value_cansleep(sensor->reset_gpio, 1);
	fsleep(20000);
	if (sensor->reset_gpio)
		dev_info(&sensor->client->dev, "reset gpio %d logical %d\n",
			 desc_to_gpio(sensor->reset_gpio),
			 gpiod_get_value_cansleep(sensor->reset_gpio));
	return 0;
}

static void hm2170_scan(struct hm2170 *sensor)
{
	unsigned int addr;
	u16 saved = sensor->client->addr;
	u8 b;

	for (addr = 0x08; addr < 0x78; addr++) {
		struct i2c_msg msgs[2];
		u8 reg[2] = { 0x00, 0x00 };

		msgs[0].addr = addr;
		msgs[0].flags = 0;
		msgs[0].len = 2;
		msgs[0].buf = reg;
		msgs[1].addr = addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = 1;
		msgs[1].buf = &b;
		if (i2c_transfer(sensor->client->adapter, msgs, 2) == 2)
			dev_info(&sensor->client->dev,
				 "i2c 0x%02x acked, reg 0x0000=0x%02x\n",
				 addr, b);
	}
	sensor->client->addr = saved;
}

static int hm2170_identify(struct hm2170 *sensor)
{
	u8 idb[3];
	u16 id;
	int ret;
	int try;

	for (try = 0; try < 5; try++) {
		ret = hm2170_read(sensor, HM2170_REG_CHIP_ID, idb, 3);
		if (!ret)
			break;
		fsleep(20000);
	}
	if (ret) {
		dev_err(&sensor->client->dev, "no ack at i2c 0x%02x (%d)\n",
			sensor->client->addr, ret);
		hm2170_scan(sensor);
		return ret;
	}
	id = ((u16)idb[0] << 8) | idb[1];
	sensor->rev = idb[2];
	if (id != HM2170_CHIP_ID) {
		dev_err(&sensor->client->dev,
			"chip id 0x%04x rev 0x%02x is not HM2170\n",
			id, sensor->rev);
		return -ENODEV;
	}
	if (sensor->rev < 4) {
		sensor->table = hm2170_ver_b;
		sensor->nregs = ARRAY_SIZE(hm2170_ver_b);
	} else {
		sensor->table = hm2170_ver_d;
		sensor->nregs = ARRAY_SIZE(hm2170_ver_d);
	}
	dev_info(&sensor->client->dev,
		 "HM2170 id 0x%04x rev 0x%02x (%s), %u lanes, link %llu Hz, xclk %u\n",
		 id, sensor->rev, sensor->rev < 4 ? "B" : "D",
		 HM2170_LANES, sensor->link_freq, HM2170_XCLK_HZ);
	return 0;
}

static int hm2170_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct hm2170 *sensor = to_hm2170(sd);
	int ret = 0;

	mutex_lock(&sensor->lock);
	if (enable == sensor->streaming)
		goto out;
	if (enable) {
		ret = hm2170_write_table(sensor);
		if (!ret)
			ret = hm2170_write(sensor, HM2170_REG_MODE,
					   HM2170_MODE_STREAMING);
	} else {
		ret = hm2170_write(sensor, HM2170_REG_MODE,
				   HM2170_MODE_STANDBY);
	}
	if (!ret)
		sensor->streaming = enable;
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_mbus_framefmt hm2170_fmt = {
	.width = HM2170_WIDTH,
	.height = HM2170_HEIGHT,
	.code = MEDIA_BUS_FMT_SGRBG10_1X10,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_RAW,
	.ycbcr_enc = V4L2_YCBCR_ENC_601,
	.quantization = V4L2_QUANTIZATION_FULL_RANGE,
	.xfer_func = V4L2_XFER_FUNC_NONE,
};

static int hm2170_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	*v4l2_subdev_get_pad_format(sd, state, 0) = hm2170_fmt;
	return 0;
}

static int hm2170_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	return 0;
}

static int hm2170_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index || fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;
	fse->min_width = fse->max_width = HM2170_WIDTH;
	fse->min_height = fse->max_height = HM2170_HEIGHT;
	return 0;
}

static int hm2170_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	fmt->format = *v4l2_subdev_get_pad_format(sd, state, fmt->pad);
	return 0;
}

static int hm2170_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	*v4l2_subdev_get_pad_format(sd, state, fmt->pad) = hm2170_fmt;
	fmt->format = hm2170_fmt;
	return 0;
}

static int hm2170_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	if (sel->target != V4L2_SEL_TGT_CROP &&
	    sel->target != V4L2_SEL_TGT_CROP_BOUNDS &&
	    sel->target != V4L2_SEL_TGT_CROP_DEFAULT)
		return -EINVAL;
	sel->r.left = 0;
	sel->r.top = 0;
	sel->r.width = HM2170_WIDTH;
	sel->r.height = HM2170_HEIGHT;
	return 0;
}

static int hm2170_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	fi->interval.numerator = 1;
	fi->interval.denominator = 30;
	return 0;
}

static int hm2170_get_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				  struct v4l2_mbus_config *config)
{
	struct v4l2_mbus_config_mipi_csi2 *mipi = &config->bus.mipi_csi2;

	if (pad)
		return -EINVAL;
	config->type = V4L2_MBUS_CSI2_DPHY;
	mipi->num_data_lanes = HM2170_LANES;
	return 0;
}

static const struct v4l2_subdev_video_ops hm2170_video_ops = {
	.s_stream = hm2170_s_stream,
	.g_frame_interval = hm2170_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops hm2170_pad_ops = {
	.init_cfg = hm2170_init_state,
	.enum_mbus_code = hm2170_enum_mbus_code,
	.enum_frame_size = hm2170_enum_frame_size,
	.get_fmt = hm2170_get_fmt,
	.set_fmt = hm2170_set_fmt,
	.get_selection = hm2170_get_selection,
	.get_mbus_config = hm2170_get_mbus_config,
};

static const struct v4l2_subdev_ops hm2170_subdev_ops = {
	.video = &hm2170_video_ops,
	.pad = &hm2170_pad_ops,
};

static int hm2170_init_controls(struct hm2170 *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	int ret;

	hm2170_link_menu[0] = sensor->link_freq;
	ret = v4l2_ctrl_handler_init(hdl, 2);
	if (ret)
		return ret;
	v4l2_ctrl_new_int_menu(hdl, NULL, V4L2_CID_LINK_FREQ, 0, 0,
			       hm2170_link_menu);
	v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_PIXEL_RATE,
			  sensor->pixel_rate, sensor->pixel_rate, 1,
			  sensor->pixel_rate);
	if (hdl->error)
		return hdl->error;
	sensor->sd.ctrl_handler = hdl;
	return 0;
}

static int hm2170_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;

	(void)id;
	struct hm2170 *sensor;
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
	sensor->pixel_rate = sensor->link_freq * 2 * HM2170_LANES / 10;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "missing CSI-2 endpoint\n");
	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret)
		return dev_err_probe(dev, ret, "failed to parse endpoint\n");
	if (ep.bus.mipi_csi2.num_data_lanes != HM2170_LANES) {
		v4l2_fwnode_endpoint_free(&ep);
		return dev_err_probe(dev, -EINVAL, "HM2170 table is 2-lane\n");
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

	v4l2_i2c_subdev_init(&sensor->sd, client, &hm2170_subdev_ops);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;
	ret = hm2170_init_controls(sensor);
	if (ret)
		goto err_entity;
	sensor->sd.state_lock = &sensor->lock;
	ret = v4l2_subdev_init_finalize(&sensor->sd);
	if (ret)
		goto err_ctrls;
	ret = hm2170_power(sensor, true);
	if (ret)
		goto err_subdev;
	ret = hm2170_identify(sensor);
	if (ret)
		goto err_power;
	ret = hm2170_write_table(sensor);
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
	hm2170_power(sensor, false);
err_subdev:
	v4l2_subdev_cleanup(&sensor->sd);
err_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_entity:
	media_entity_cleanup(&sensor->sd.entity);
	return ret;
}

static void hm2170_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hm2170 *sensor = to_hm2170(sd);

	v4l2_async_unregister_subdev(sd);
	hm2170_power(sensor, false);
	v4l2_subdev_cleanup(sd);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
	media_entity_cleanup(&sd->entity);
	mutex_destroy(&sensor->lock);
}

static const struct of_device_id hm2170_of_match[] = {
	{ .compatible = "himax,hm2170" },
	{ }
};
MODULE_DEVICE_TABLE(of, hm2170_of_match);

static const struct i2c_device_id hm2170_id[] = {
	{ "hm2170", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hm2170_id);

static struct i2c_driver hm2170_i2c_driver = {
	.driver = {
		.name = "hm2170",
		.of_match_table = hm2170_of_match,
	},
	.probe = hm2170_probe,
	.remove = hm2170_remove,
	.id_table = hm2170_id,
};
module_i2c_driver(hm2170_i2c_driver);

MODULE_DESCRIPTION("Himax HM2170 CSI-2 sensor driver");
MODULE_LICENSE("GPL");
