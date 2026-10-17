/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <finch/obc_flash/util.h>

/* Register logging module */
LOG_MODULE_REGISTER(spi_jedec);

/* SPI controller */
#define SPI_NODE DT_NODELABEL(spi1)

/* Manual CS GPIO. Controlling CS pin manually. */
#define CS_GPIO_NODE DT_NODELABEL(gpioa)
const char *gpio_set = "gpioa"; /* Used in log messages */

//#define CS_PIN  4
#define WAIT_MS 1000

#define FLASH_CMD_WRENB 0x06 // write enable, must be sent before any write/erase operation
#define FLASH_CMD_RDSR1 0x05 // read status register
#define FLASH_CMD_SE    0x20 // sector erase
#define FLASH_CMD_PP    0x02 // page program, writing data into flash mem
#define FLASH_CMD_READ  0x03 // read bytes from a given mem address
#define FLASH_CMD_ULBPR 0x98 // unblock all protections
#define FLASH_CMD_RESET_EN 0x66 // reset enable
#define FLASH_CMD_RESET 0x99    // reset memory, must be sent after RESET_EN
#define FLASH_CMD_ERASE_FULL 0xC7 // erase whole flash memorys

#define FLASH_STATUS_BUSY     BIT(0)
#define FLASH_POLL_TIMEOUT_MS 10000

static const struct spi_config spi_cfg = {
	.frequency = 1875000,                            /* 1 MHz */
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB, /* MSB/LSB doesn't matter if sending 1
							    byte. Should be MSB still */
	.slave = 0,                                      /* ignored when CS is manual */
};

static int flash_xfer(const struct device *spi, const struct device *gpio, int cs_pin, const uint8_t *tx,
		      uint8_t *rx, size_t len)
{
	uint8_t dummy_rx[len];

	struct spi_buf tx_buf = {.buf = (void *)tx, .len = len};
	struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
	struct spi_buf rx_buf = {.buf = (rx != NULL) ? rx : dummy_rx, .len = len};
	struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};

	int ret = gpio_pin_set(gpio, cs_pin, 0);
	if (ret != 0) {
		return ret;
	}

	k_busy_wait(1);

	ret = spi_transceive(spi, &spi_cfg, &tx_set, &rx_set);

	k_busy_wait(1);

	int cs_ret = gpio_pin_set(gpio, cs_pin, 1);
	if ((ret == 0) && (cs_ret != 0)) {
		ret = cs_ret;
	}

	k_busy_wait(1);

	return ret;
}

static int flash_read_status(const struct device *spi, const struct device *gpio, int cs_pin, uint8_t *status)
{
	uint8_t tx[2] = {FLASH_CMD_RDSR1, 0x00};
	uint8_t rx[2] = {0};

	int ret = flash_xfer(spi, gpio, cs_pin, tx, rx, sizeof(tx));
	if (ret != 0) {
		return ret;
	}

	*status = rx[1];
	return 0;
}

static int flash_wait_ready(const struct device *spi, const struct device *gpio, int cs_pin)
{
	k_msleep(10);

	// FLASH_POLL_TIMEOUT_MS
	for (int elapsed = 0; elapsed < FLASH_POLL_TIMEOUT_MS; elapsed++) {
		uint8_t status = 0;
		int ret = flash_read_status(spi, gpio, cs_pin, &status);
		if (ret != 0) {
			return ret;
		}

		if ((status & FLASH_STATUS_BUSY) == 0U) {
			return 0;
		}

		k_msleep(1);
	}

	LOG_ERR("Timed out waiting for flash ready");
	return -ETIMEDOUT;
}

int obc_flash_global_unlock(const struct device *spi, const struct device *gpio, int cs_pin)
{
	uint8_t wren = FLASH_CMD_WRENB;
	uint8_t ulbpr = FLASH_CMD_ULBPR;

	int ret = flash_xfer(spi, gpio, cs_pin, &wren, NULL, 1);
	if (ret != 0) {
		return ret;
	}

	ret = flash_xfer(spi, gpio, cs_pin, &ulbpr, NULL, 1);
	if (ret != 0) {
		return ret;
	}

	return flash_wait_ready(spi, gpio, cs_pin);
}

int obc_erase_sector(const struct device *spi, const struct device *gpio, int cs_pin, uint32_t addr)
{
    uint8_t wren = FLASH_CMD_WRENB;

    uint8_t erase_cmd[4] = {
        FLASH_CMD_SE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
    };

    int ret = flash_xfer(spi, gpio, cs_pin, &wren, NULL, 1);
    if (ret != 0) {
        LOG_ERR("WREN before sector erase failed (ret=%d)", ret);
        return ret;
    }

    ret = flash_xfer(spi, gpio, cs_pin, erase_cmd, NULL, sizeof(erase_cmd));
    if (ret != 0) {
        LOG_ERR("Sector erase command failed (ret=%d)", ret);
        return ret;
    }

    return flash_wait_ready(spi, gpio, cs_pin);
}

int obc_prog(const struct device *spi, const struct device *gpio, int cs_pin, uint32_t addr, const uint8_t *data)
{
    uint8_t wren = FLASH_CMD_WRENB;
    uint8_t program_cmd[8] = {
        FLASH_CMD_PP,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
        data[0],
        data[1],
        data[2],
        data[3],
    };

    int ret = flash_xfer(spi, gpio, cs_pin, &wren, NULL, 1);
    if (ret != 0) {
        LOG_ERR("WREN before program failed (ret=%d)", ret);
        return ret;
    }

    ret = flash_xfer(spi, gpio, cs_pin, program_cmd, NULL, sizeof(program_cmd));
    if (ret != 0) {
        LOG_ERR("Page program command failed (ret=%d)", ret);
        return ret;
    }

    return flash_wait_ready(spi, gpio, cs_pin);
}

int obc_read(const struct device *spi, const struct device *gpio, int cs_pin, uint32_t addr, uint8_t *result)
{

	uint8_t read_cmd[8] = {
		FLASH_CMD_READ,
		(uint8_t)(addr >> 16),
		(uint8_t)(addr >> 8),
		(uint8_t)addr,
		0x00,
		0x00,
		0x00,
		0x00,
	};
	uint8_t read_rx[8] = {0};

	int ret = flash_xfer(spi, gpio, cs_pin, read_cmd, read_rx, sizeof(read_cmd));
	if (ret != 0) {
		LOG_ERR("Readback command failed (ret=%d)", ret);
		return ret;
	}

	result[0] = read_rx[4];
	result[1] = read_rx[5];
	result[2] = read_rx[6];
	result[3] = read_rx[7];

	return 0;
}

int obc_print_id(const struct device *spi, const struct device *gpio, int cs_pin)
{

	LOG_INF("SPI JEDEC test (manual CS)");
	LOG_INF("Configuring the CS Pin: %s-%d", gpio_set, cs_pin);



	uint8_t cmd[6] = {0x9F}; /* JEDEC READ ID command */
	uint8_t rx[6] = {0};

    int ret = flash_xfer(spi, gpio, cs_pin, cmd, rx, sizeof(cmd));
    if (ret != 0) {
        LOG_ERR("JEDEC id print failed (ret=%d)", ret);
        return ret;
    }

	LOG_INF("Raw response: %02X %02X %02X %02X %02X %02X", rx[0], rx[1], rx[2], rx[3], rx[4],
		rx[5]);
	LOG_INF("JEDEC ID: %02X %02X %02X", rx[1], rx[2], rx[3]);

	return 0;
}

// cppcheck-suppress unusedFunction
int obc_reset_state(const struct device *spi, const struct device *gpio, int cs_pin)
{
    uint8_t reset_en = FLASH_CMD_RESET_EN;
    uint8_t reset = FLASH_CMD_RESET;

    int ret = flash_xfer(spi, gpio, cs_pin, &reset_en, NULL, 1);
    if (ret != 0) {
        LOG_ERR("Reset enable command failed (ret=%d)", ret);
        return ret;
    }

    ret = flash_xfer(spi, gpio, cs_pin, &reset, NULL, 1);
    if (ret != 0) {
        LOG_ERR("Reset command failed (ret=%d)", ret);
        return ret;
    }

    return flash_wait_ready(spi, gpio, cs_pin);
}

// cppcheck-suppress unusedFunction
int obc_erase_all_mem(const struct device *spi, const struct device *gpio, int cs_pin)
{
    uint8_t wren = FLASH_CMD_WRENB;
    uint8_t erase_full = FLASH_CMD_ERASE_FULL;

    int ret = flash_xfer(spi, gpio, cs_pin, &wren, NULL, 1);
    if (ret != 0) {
        LOG_ERR("WREN before full erase failed (ret=%d)", ret);
        return ret;
    }

    ret = flash_xfer(spi, gpio, cs_pin, &erase_full, NULL, 1);
    if (ret != 0) {
        LOG_ERR("Full erase command failed (ret=%d)", ret);
        return ret;
    }

    return flash_wait_ready(spi, gpio, cs_pin);
}
