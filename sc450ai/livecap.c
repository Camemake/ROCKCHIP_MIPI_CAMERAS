/* Stream half-resolution RGB from the Aura SC450AI CIF to stdout.
 * The CIF writes four 10-bit pixels, little-endian, in every five bytes.
 * The sensor is BGGR.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define WIDTH 2688
#define HEIGHT 1520
#define OUT_W (WIDTH / 2)
#define OUT_H (HEIGHT / 2)
#define OUT_BYTES (OUT_W * OUT_H * 3)
#define BUFFERS 4

static uint8_t gamma_lut[256];

static void init_gamma(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		float x = powf(i / 255.0f, 1.0f / 1.6f);
		int v = (int)(x * 255.0f + 0.5f);
		if (v > 255)
			v = 255;
		gamma_lut[i] = (uint8_t)v;
	}
}

static int lift(int v)
{
	if (v <= 8)
		return 0;
	return (v - 8) * 255 / 247;
}

static void unpack_row(const uint8_t *src, uint8_t *dst)
{
	int x = 0;
	int g;

	for (g = 0; x < WIDTH; g += 5) {
		uint64_t v = (uint64_t)src[g]
			| ((uint64_t)src[g + 1] << 8)
			| ((uint64_t)src[g + 2] << 16)
			| ((uint64_t)src[g + 3] << 24)
			| ((uint64_t)src[g + 4] << 32);
		dst[x++] = (uint8_t)((v & 0x3ff) >> 2);
		dst[x++] = (uint8_t)(((v >> 10) & 0x3ff) >> 2);
		dst[x++] = (uint8_t)(((v >> 20) & 0x3ff) >> 2);
		dst[x++] = (uint8_t)(((v >> 30) & 0x3ff) >> 2);
	}
}

static void debayer(const uint8_t *frame, unsigned int stride, uint8_t *rgb)
{
	uint8_t row0[WIDTH], row1[WIDTH];
	uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
	unsigned int n = 0;
	int y, x, i;
	float mid, sr, sg, sb;

	for (y = 0; y < HEIGHT; y += 2) {
		uint8_t *out = rgb + (y / 2) * OUT_W * 3;

		unpack_row(frame + (unsigned int)y * stride, row0);
		unpack_row(frame + (unsigned int)(y + 1) * stride, row1);
		for (x = 0; x < WIDTH; x += 2) {
			int b = lift(row0[x]);
			int g = lift((row0[x + 1] + row1[x]) / 2);
			int r = lift(row1[x + 1]);

			*out++ = (uint8_t)r;
			*out++ = (uint8_t)g;
			*out++ = (uint8_t)b;
			sum_r += r;
			sum_g += g;
			sum_b += b;
			n++;
		}
	}

	mid = (sum_r + sum_g + sum_b) / (3.0f * n);
	sr = mid / ((float)sum_r / n);
	sg = mid / ((float)sum_g / n);
	sb = mid / ((float)sum_b / n);
	if (sr < 0.55f) sr = 0.55f;
	if (sg < 0.55f) sg = 0.55f;
	if (sb < 0.55f) sb = 0.55f;
	if (sr > 1.8f) sr = 1.8f;
	if (sg > 1.8f) sg = 1.8f;
	if (sb > 1.8f) sb = 1.8f;

	for (i = 0; i < OUT_BYTES; i += 3) {
		int r = (int)(rgb[i] * sr + 0.5f);
		int g = (int)(rgb[i + 1] * sg + 0.5f);
		int b = (int)(rgb[i + 2] * sb + 0.5f);
		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		rgb[i] = gamma_lut[r];
		rgb[i + 1] = gamma_lut[g];
		rgb[i + 2] = gamma_lut[b];
	}
}

int main(int argc, char **argv)
{
	const char *node = argc > 1 ? argv[1] : "/dev/video28";
	int fd, i, type;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	void *start[BUFFERS];
	unsigned int length[BUFFERS];
	unsigned int stride, sizeimage;
	uint8_t *rgb;

	init_gamma();
	rgb = malloc(OUT_BYTES);
	if (!rgb)
		return 1;

	fd = open(node, O_RDWR);
	if (fd < 0) {
		perror(node);
		return 1;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = WIDTH;
	fmt.fmt.pix_mp.height = HEIGHT;
	fmt.fmt.pix_mp.pixelformat = v4l2_fourcc('B', 'G', '1', '0');
	fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	fmt.fmt.pix_mp.num_planes = 1;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		perror("VIDIOC_S_FMT");
		return 1;
	}
	stride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	sizeimage = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
	fprintf(stderr, "sizeimage %u bytesperline %u\n", sizeimage, stride);

	memset(&req, 0, sizeof(req));
	req.count = BUFFERS;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		perror("VIDIOC_REQBUFS");
		return 1;
	}

	for (i = 0; i < BUFFERS; i++) {
		struct v4l2_buffer buf;
		struct v4l2_plane planes[1];

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = 1;
		buf.m.planes = planes;
		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
			perror("VIDIOC_QUERYBUF");
			return 1;
		}
		length[i] = planes[0].length;
		start[i] = mmap(NULL, planes[0].length, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, planes[0].m.mem_offset);
		if (start[i] == MAP_FAILED) {
			perror("mmap");
			return 1;
		}
		if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
			return 1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		perror("VIDIOC_STREAMON");
		return 1;
	}
	fprintf(stderr, "streaming\n");

	for (;;) {
		struct v4l2_buffer buf;
		struct v4l2_plane planes[1];

		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1;
		buf.m.planes = planes;
		if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
			if (errno == EINTR)
				continue;
			perror("VIDIOC_DQBUF");
			return 1;
		}
		debayer(start[buf.index], stride, rgb);
		if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
			return 1;
		}
		if (fwrite(rgb, 1, OUT_BYTES, stdout) != OUT_BYTES)
			return 0;
		fflush(stdout);
	}
}
