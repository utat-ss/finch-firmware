/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <finch/obc_flash/util.h>
#include <lfs.h>
#include <lfs_util.h>
#include <finch/obc_littlefs/lfs_callbacks.h>

#include <zephyr/ztest.h>

/* SPI controller */
#define SPI_NODE DT_NODELABEL(spi1)
/* Manual CS GPIO. Controlling CS pin manually. */
#define CS_GPIO_NODE DT_NODELABEL(gpioa)
#define CS_PIN       4
static const struct device *spi;
static const struct device *gpio;

// Filesystem vars
lfs_t lfs;
lfs_file_t file;

struct lfs_config cfg = {
    // block device operations
    .read  = lfs_cb_read,
    .prog  = lfs_cb_prog,
    .erase = lfs_cb_erase,
    .sync  = lfs_cb_sync,

    // block device configuration
    .read_size = 4,
    .prog_size = 4,
    .block_size = 4096,
    .block_count = 2048,
    .block_cycles = 500,
    .cache_size = 256,
    .lookahead_size = 16,
    .compact_thresh = 0,
};

struct lfs_flash_context flash_context;

static void *obc_littlefs_setup(void)
{
    // Setting up SPI and GPIO devices for flash operations
	spi = DEVICE_DT_GET(SPI_NODE);
	gpio = DEVICE_DT_GET(CS_GPIO_NODE);

	zassert_true(device_is_ready(spi), "SPI device is not ready");
	zassert_true(device_is_ready(gpio), "GPIO device is not ready");

	int ret = gpio_pin_configure(gpio, CS_PIN, GPIO_OUTPUT_HIGH);
	zassert_equal(ret, 0, "Failed to configure CS pin");

    ret = obc_flash_global_unlock(spi, gpio, CS_PIN);
    zassert_equal(ret, 0, "Failed to unlock flash");

    flash_context.spi = spi;
    flash_context.gpio = gpio;
    flash_context.cs_pin = CS_PIN;

    cfg.context = &flash_context;

    // mount
    int err = lfs_mount(&lfs, &cfg);
    
    if (err) {
        // If mount fails, format filesystem and retry mount
        err = lfs_format(&lfs, &cfg);
        zassert_equal(err, 0, "Failed to format the filesystem");

        err = lfs_mount(&lfs, &cfg);
        zassert_equal(err, 0, "Failed to mount filesystem after formatting");
    }

	return NULL;
}

ZTEST_SUITE(obc_littlefs, NULL, obc_littlefs_setup, NULL, NULL, NULL);

ZTEST(obc_littlefs, test_flash_setup)
{
    zassert_not_null(cfg.context, "Flash context was not configured");
}

ZTEST(obc_littlefs, test_file_write_and_read)
{
    const char path[] = "/sanity.txt";
    const char expected[] = "hello littlefs";
    char actual[sizeof(expected)] = {0};
    lfs_file_t test_file;

    // Remove a copy left behind by an earlier failed test run.
    lfs_remove(&lfs, path);

    int err = lfs_file_open(&lfs, &test_file, path,
            LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    zassert_equal(err, 0, "Failed to create file: %d", err);

    lfs_ssize_t size = lfs_file_write(&lfs, &test_file,
            expected, sizeof(expected));
    zassert_equal(size, sizeof(expected), "Failed to write file: %d", (int)size);

    err = lfs_file_close(&lfs, &test_file);
    zassert_equal(err, 0, "Failed to close file: %d", err);

    err = lfs_file_open(&lfs, &test_file, path, LFS_O_RDONLY);
    zassert_equal(err, 0, "Failed to reopen file: %d", err);

    size = lfs_file_read(&lfs, &test_file, actual, sizeof(actual));
    zassert_equal(size, sizeof(actual), "Failed to read file: %d", (int)size);
    zassert_mem_equal(actual, expected, sizeof(expected), "File contents did not match");

    err = lfs_file_close(&lfs, &test_file);
    zassert_equal(err, 0, "Failed to close file: %d", err);

    err = lfs_remove(&lfs, path);
    zassert_equal(err, 0, "Failed to remove file: %d", err);
}

ZTEST(obc_littlefs, test_create_directory)
{
    const char path[] = "/sanity_dir";
    struct lfs_info info;

    // Remove a copy left behind by an earlier failed test run.
    lfs_remove(&lfs, path);

    int err = lfs_mkdir(&lfs, path);
    zassert_equal(err, 0, "Failed to create directory: %d", err);

    err = lfs_stat(&lfs, path, &info);
    zassert_equal(err, 0, "Failed to stat directory: %d", err);
    zassert_equal(info.type, LFS_TYPE_DIR, "Created path was not a directory");

    err = lfs_remove(&lfs, path);
    zassert_equal(err, 0, "Failed to remove directory: %d", err);
}
