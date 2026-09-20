/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <finch/csp/csp.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(pay);

/*
 * PAY camera-to-storage draft
 * --------------------------------
 * 1. Configure the OV7670 through I2C.
 * 2. Capture an RGB565 frame through the STM32 DCMI peripheral and DMA.
 * 3. Write the raw frame to the FAT-formatted SD card through SDMMC1.
 * 4. Return the frame buffer to DCMI so that it can be reused.
 *
 * The files --> raw RGB565 pixels without an image-file header.
 * A ground tool must therefore know the width, height, pixel format, and byte order.
 */

/* The chosen camera is DCMI, which receives pixels from the OV7670 sensor. */
#define CAMERA_NODE       DT_CHOSEN(zephyr_camera)
/* Devicetree entry describing the FAT filesystem mounted on the SD card. */
#define SD_FSTAB_NODE     DT_NODELABEL(sd_card)
/* QVGA RGB565 uses two bytes per pixel: 320 * 240 * 2 = 153600 bytes. */
#define FRAME_WIDTH       320U
#define FRAME_HEIGHT      240U
/* Keep names compatible with FAT 8.3 format: IMG00000.RAW to IMG99999.RAW. */
#define FRAME_COUNT_LIMIT 100000U
#define FRAME_PATH_SIZE   sizeof("/SD:/IMG99999.RAW")

/* Make the mount structure generated from the devicetree visible here. */
FS_FSTAB_DECLARE_ENTRY(SD_FSTAB_NODE);

/** Mount the existing FAT filesystem without formatting the card. */
static int mount_sd_card(void)
{
	struct fs_mount_t *mount = &FS_FSTAB_ENTRY(SD_FSTAB_NODE);
	int ret;

	ret = fs_mount(mount);
	if (ret < 0) {
		LOG_ERR("Failed to mount SD card at %s (%d)", mount->mnt_point, ret);
		return ret;
	}

	LOG_INF("Mounted SD card at %s", mount->mnt_point);
	return 0;
}

/** Configure DCMI and give the video driver the buffers used for DMA capture. */
static int setup_camera(const struct device *camera, struct video_format *format)
{
	/*
	 * Zephyr calls the capture direction OUTPUT because frames leave the
	 * camera device and are delivered to this application.
	 */
	struct video_caps caps = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
	};
	int ret;

	if (!device_is_ready(camera)) {
		LOG_ERR("Camera device %s is not ready", camera->name);
		return -ENODEV;
	}

	/* Ask the complete DCMI/OV7670 pipeline how many buffers it requires. */
	ret = video_get_caps(camera, &caps);
	if (ret < 0) {
		LOG_ERR("Failed to query camera capabilities (%d)", ret);
		return ret;
	}

	if (caps.min_vbuf_count > CONFIG_VIDEO_BUFFER_POOL_NUM_MAX) {
		LOG_ERR("Camera needs %u buffers, but only %u are configured", caps.min_vbuf_count,
			CONFIG_VIDEO_BUFFER_POOL_NUM_MAX);
		return -ENOMEM;
	}

	/* This call configures both DCMI and the connected OV7670 sensor. */
	ret = video_set_format(camera, format);
	if (ret < 0) {
		LOG_ERR("Failed to set %ux%u RGB565 camera format (%d)", FRAME_WIDTH, FRAME_HEIGHT,
			ret);
		return ret;
	}

	if (format->size > CONFIG_VIDEO_BUFFER_POOL_SZ_MAX) {
		LOG_ERR("Frame size %u exceeds the configured buffer size %u", format->size,
			CONFIG_VIDEO_BUFFER_POOL_SZ_MAX);
		return -ENOMEM;
	}

	/* Allocate and queue every buffer before starting continuous capture. */
	for (size_t i = 0; i < CONFIG_VIDEO_BUFFER_POOL_NUM_MAX; ++i) {
		struct video_buffer *buffer;

		buffer = video_buffer_aligned_alloc(format->size, CONFIG_VIDEO_BUFFER_POOL_ALIGN,
						    K_NO_WAIT);
		if (buffer == NULL) {
			LOG_ERR("Failed to allocate camera buffer %u", (unsigned int)i);
			return -ENOMEM;
		}

		buffer->type = VIDEO_BUF_TYPE_OUTPUT;
		ret = video_enqueue(camera, buffer);
		if (ret < 0) {
			LOG_ERR("Failed to queue camera buffer %u (%d)", (unsigned int)i, ret);
			return ret;
		}
	}

	/* DCMI now fills queued buffers through DMA as frames arrive. */
	ret = video_stream_start(camera, VIDEO_BUF_TYPE_OUTPUT);
	if (ret < 0) {
		LOG_ERR("Failed to start DCMI capture (%d)", ret);
		return ret;
	}

	LOG_INF("DCMI capture started: %ux%u RGB565, %u bytes per frame", format->width,
		format->height, format->size);
	return 0;
}

/** Store one captured frame as a raw RGB565 file on the SD card. */
static int write_frame(const struct video_buffer *frame, uint32_t frame_number)
{
	char path[FRAME_PATH_SIZE];
	struct fs_file_t file;
	ssize_t written;
	int close_ret;
	int ret;

	/* FS_O_TRUNC replaces an older frame if the five-digit number wraps. */
	snprintk(path, sizeof(path), "/SD:/IMG%05u.RAW", frame_number);
	fs_file_t_init(&file);

	ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret < 0) {
		LOG_ERR("Failed to open %s (%d)", path, ret);
		return ret;
	}

	written = fs_write(&file, frame->buffer, frame->bytesused);
	if (written < 0) {
		LOG_ERR("Failed to write %s (%d)", path, (int)written);
		ret = (int)written;
	} else if ((size_t)written != frame->bytesused) {
		LOG_ERR("Short write to %s: %d of %u bytes", path, (int)written, frame->bytesused);
		ret = -EIO;
	}

	/* Closing commits filesystem metadata even when the preceding write failed. */
	close_ret = fs_close(&file);

	if (ret == 0 && close_ret < 0) {
		LOG_ERR("Failed to close %s (%d)", path, close_ret);
		ret = close_ret;
	}

	if (ret == 0) {
		LOG_INF("Saved %s (%u bytes)", path, frame->bytesused);
	}

	return ret;
}

int main(void)
{
	/* CAMERA_NODE resolves to the STM32 DCMI controller, not the I2C sensor. */
	const struct device *camera = DEVICE_DT_GET(CAMERA_NODE);
	struct video_format format = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
		.pixelformat = VIDEO_PIX_FMT_RGB565,
		.width = FRAME_WIDTH,
		.height = FRAME_HEIGHT,
	};
	uint32_t frame_number = 0;
	int ret;

	/*
	 * CSP? still drk if this is ok
	 */
	ret = finch_csp_init();

	if (ret < 0) {
		/* TODO: FDIR should retry CSP or select a documented degraded mode. */
		LOG_ERR("Failed to initialize FINCH CSP (%d)", ret);
		return ret;
	}

	/* Validate storage before starting the camera and consuming frame buffers. */
	ret = mount_sd_card();
	if (ret < 0) {
		return ret;
	}

	/* Configure the sensor, DCMI, DMA, and video-buffer pool. */
	ret = setup_camera(camera, &format);
	if (ret < 0) {
		fs_unmount(&FS_FSTAB_ENTRY(SD_FSTAB_NODE));
		return ret;
	}

	/* Capture and save frames continuously until any side reports an error. */
	while (1) {
		struct video_buffer *frame = NULL;
		int queue_ret;

		/* Wait until DCMI/DMA has completed a frame. */
		ret = video_dequeue(camera, &frame, K_FOREVER);
		if (ret < 0) {
			LOG_ERR("Failed to dequeue a DCMI frame (%d)", ret);
			break;
		}

		ret = write_frame(frame, frame_number);

		/* Always return the buffer, including after an SD write failure. */
		queue_ret = video_enqueue(camera, frame);
		if (queue_ret < 0) {
			LOG_ERR("Failed to return DCMI frame buffer (%d)", queue_ret);
			ret = queue_ret;
			break;
		}
		if (ret < 0) {
			break;
		}

		/* Reuse the oldest filename after IMG99999.RAW. */
		frame_number = (frame_number + 1U) % FRAME_COUNT_LIMIT;
	}

	/* Best-effort shutdown keeps the SD filesystem consistent after an error. */
	video_stream_stop(camera, VIDEO_BUF_TYPE_OUTPUT);
	fs_unmount(&FS_FSTAB_ENTRY(SD_FSTAB_NODE));
	return ret;
}
