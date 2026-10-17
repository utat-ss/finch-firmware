/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#define FLASH_SPI_OPERATION (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB)

BUILD_ASSERT(!IS_ENABLED(CONFIG_FINCH_OBC_FLASH) &&
	     !IS_ENABLED(CONFIG_FINCH_OBC_LITTLEFS) && !IS_ENABLED(CONFIG_FLASH) &&
	     !IS_ENABLED(CONFIG_FILE_SYSTEM), "This test must remain standalone and read-only");

static const struct spi_dt_spec flashes[] = {
	SPI_DT_SPEC_GET(DT_NODELABEL(ext_flash0), FLASH_SPI_OPERATION),
	SPI_DT_SPEC_GET(DT_NODELABEL(ext_flash1), FLASH_SPI_OPERATION),
};

/* Only these four read commands may reach the bus. No flash driver is enabled.
 * RDID/RDSR2/RDCR require factory VRGLAT=0 (CFR3V[7:6]). No mode changes.
 */
static int read_command(size_t index, uint8_t command, uint8_t *data, size_t length)
{
	uint8_t tx[7] = {command};
	uint8_t rx[sizeof(tx)] = {0};

	if (index >= ARRAY_SIZE(flashes) || data == NULL || length >= sizeof(tx) ||
	    !((command == 0x9f && length == 6) ||
	      ((command == 0x05 || command == 0x07 || command == 0x35) && length == 1))) {
		return -EINVAL;
	}

	struct spi_buf tx_buf = {.buf = tx, .len = length + 1};
	struct spi_buf rx_buf = {.buf = rx, .len = length + 1};
	const struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
	const struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
	int ret = spi_transceive_dt(&flashes[index], &tx_set, &rx_set);

	if (ret == 0) {
		memcpy(data, &rx[1], length);
	}
	return ret;
}

static void *setup(void)
{
	/* Deassert BOTH CS lines before either chip receives clocks. */
	for (size_t i = 0; i < ARRAY_SIZE(flashes); i++) {
		zassert_true(spi_cs_is_gpio_dt(&flashes[i]), "GPIO chip select missing");
		zassert_true(spi_is_ready_dt(&flashes[i]), "SPI or PE%u CS not ready",
			     (unsigned int)(i + 4));
		zassert_equal(flashes[i].config.cs.gpio.port, DEVICE_DT_GET(DT_NODELABEL(gpioe)),
			      "Expected GPIOE chip select");
		zassert_equal(flashes[i].config.cs.gpio.pin, i + 4, "Expected PE4/PE5");
		zassert_true(flashes[i].config.cs.gpio.dt_flags & GPIO_ACTIVE_LOW,
			     "Expected active-low CS");
		int ret = gpio_pin_configure_dt(&flashes[i].config.cs.gpio,
					       GPIO_OUTPUT_INACTIVE);

		zassert_ok(ret, "PE%u CS configuration failed (%d)",
			   (unsigned int)(i + 4), ret);
	}
	k_msleep(100);
	printk("External flash read-only test: SPI1, PE4/PE5, mode 0, requested 1875000 Hz\n");
	printk("Requires 1-1-1 SPI and factory zero register latency; no settings changed\n");
	return NULL;
}

static void identify(size_t index)
{
	uint8_t id[6] = {0};
	uint8_t sr1 = 0, sr2 = 0, cr1 = 0;
	unsigned int chip = (unsigned int)index;
	int ret = read_command(index, 0x9f, id, sizeof(id));

	zassert_ok(ret, "Flash %u RDID failed (%d)", chip, ret);
	printk("Flash %u (PE%u) ID: %02X %02X %02X %02X %02X %02X\n",
	       chip, chip + 4, (unsigned int)id[0], (unsigned int)id[1],
	       (unsigned int)id[2], (unsigned int)id[3], (unsigned int)id[4],
	       (unsigned int)id[5]);
	zassert_equal(id[0], 0x34, "Unexpected manufacturer (or wiring/mode/latency)");
	zassert_true(id[1] == 0x2a || id[1] == 0x2b, "Expected SEMPER HL-T/HS-T");
	zassert_true(id[2] == 0x1a || id[2] == 0x1b, "Expected 512 Mbit or 1 Gbit");
	zassert_equal(id[3], 0x0f, "Unexpected ID length");
	zassert_equal(id[5], 0x90, "Unexpected SEMPER family");

	ret = read_command(index, 0x05, &sr1, 1);
	zassert_ok(ret, "Flash %u RDSR1 failed (%d)", chip, ret);
	ret = read_command(index, 0x07, &sr2, 1);
	zassert_ok(ret, "Flash %u RDSR2 failed (%d)", chip, ret);
	ret = read_command(index, 0x35, &cr1, 1);
	zassert_ok(ret, "Flash %u RDCR1 failed (%d)", chip, ret);
	printk("Flash %u SR1=%02X SR2=%02X CFR1V=%02X (raw, zero latency)\n",
	       chip, (unsigned int)sr1, (unsigned int)sr2, (unsigned int)cr1);
	printk("Flash %u BUSY=%u WEL=%u; status/configuration recorded, not modified\n",
	       chip, (unsigned int)!!(sr1 & BIT(0)), (unsigned int)!!(sr1 & BIT(1)));
	/* A valid ID is distinct from a clean/idle device. Never clear error flags. */
	zassert_equal(sr1 & (BIT(0) | BIT(1) | BIT(5) | BIT(6)), 0,
		      "Flash busy, write-enabled, or has erase/program error flags; record SR1");
}

ZTEST(external_flash_id, test_flash_0_identification)
{
	identify(0);
}

ZTEST(external_flash_id, test_flash_1_identification)
{
	identify(1);
}

ZTEST_SUITE(external_flash_id, NULL, setup, NULL, NULL, NULL);
