/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <finch/adcs/adcs.h>
#include <finch/csp/csp.h>

#include <finch/obc_flash/util.h>
#include <lfs.h>
#include <lfs_util.h>
#include <finch/obc_littlefs/lfs_callbacks.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "adcs_service.h"

LOG_MODULE_REGISTER(obc);

/* SPI controller */
#define SPI_NODE DT_NODELABEL(spi1)
/* Manual CS GPIO. Controlling CS pin manually. */
#define CS_GPIO_NODE DT_NODELABEL(gpioa)
#define CS_PIN       4


// Filesystem configs
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

int main(void)
{
	int csp_rc;
	adcs_rc_t adcs_rc;

	LOG_INF("Board Started");

	/*
	 *	Get internal watchdog from device tree. watchdog0 holdes the address to iwdg, which defines the device in the device tree
	 */
	const struct device *wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
	int wdt_channel_id;

    // configuring SPI and GPIO devices for flash operations
    const struct device *spi = DEVICE_DT_GET(SPI_NODE);
    const struct device *gpio = DEVICE_DT_GET(CS_GPIO_NODE);
    int ret = gpio_pin_configure(gpio, CS_PIN, GPIO_OUTPUT_HIGH);
    // unlock flash memory to allow write/erase operations
    ret = obc_flash_global_unlock(spi, gpio, CS_PIN);
    // configure flash context for littlefs
    flash_context.spi = spi;
    flash_context.gpio = gpio;
    flash_context.cs_pin = CS_PIN;
    cfg.context = &flash_context;
    // mount littlefs filesystem, format if mount fails
    int fs_err = lfs_mount(&lfs, &cfg);
    if (fs_err) {
        LOG_INF("Failed to mount filesystem, formatting and retrying...");
        fs_err = lfs_format(&lfs, &cfg);
        // retry mounting after formatting
        fs_err = lfs_mount(&lfs, &cfg);
        if (fs_err) {
            LOG_ERR("Failed to mount filesystem after formatting");
            return fs_err;
        }
    }

	if (!device_is_ready(wdt)) {
		LOG_ERR("%s: device is not ready", wdt->name);
		return 0;
	}

	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog downcounter reaches 0. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Set min and max time window for feeding the timer */
		.window.min = CONFIG_WDT_MIN_WINDOW,
		.window.max = CONFIG_WDT_MAX_WINDOW,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);

	/* Timeout configuration setup */

	if (wdt_channel_id < 0) {
		LOG_ERR("Internal watchdog install error");
		return 0;
	}

	int err = wdt_setup(wdt, CONFIG_WDT_OPT);

	if (err < 0) {
		LOG_ERR("Internal watchdog setup error");
		return 0;
	}

	adcs_rc = adcs_init();
	if (adcs_rc != ADCS_RC_OK) {
		LOG_ERR("Failed to initialize ADCS (%d)", adcs_rc);
		return adcs_rc;
	}

	LOG_INF("ADCS READY");

	csp_rc = finch_csp_init();
	if (csp_rc != 0) {
		LOG_ERR("Failed to initialize CSP (%d)", csp_rc);
		return csp_rc;
	}

	LOG_INF("CSP READY");

	csp_rc = adcs_service_init();
	if (csp_rc != 0) {
		LOG_ERR("Failed to initialize CSP ADCS service (%d)", csp_rc);
		return csp_rc;
	}

	LOG_INF("ADCS CSP SERVICE READY");

	while (1) {

		/*
		 *	Main loop runs here. If it hangs, feeding will not run
		 */

		int ret = wdt_feed(wdt, wdt_channel_id);

		if (ret < 0) {
			LOG_ERR("Failed to feed the watchdog");
			return 0;
		}
		k_msleep(CONFIG_WDT_CHECK_INTERVAL);
	}

	return 0;
}
