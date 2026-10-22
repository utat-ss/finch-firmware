/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <finch/adcs/adcs.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

uint8_t const ADCS_ID_SIZE = 12;
static uint8_t const adcs_id_expected[] = {0x54, 0x41, 0x44, 0x31, 0x30, 0x32,
					   0x30, 0x35, 0x33, 0x44, 0x00, 0x00};

/* ADCS is connected via USART2. */
static const struct device *const adcs_uart = DEVICE_DT_GET(DT_ALIAS(adcs_uart));

/* Global buffer to store ADCS RX bytes. */
#define ADCS_RX_BUF_SIZE 32
static uint8_t adcs_rx_buf[ADCS_RX_BUF_SIZE];
static uint8_t adcs_rx_len;
static uint8_t adcs_rx_expected;
static K_SEM_DEFINE(adcs_rx_sem, 0, 1);

LOG_MODULE_REGISTER(adcs);

static void adcs_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (!uart_irq_update(dev)) {
		return;
	}

	while (uart_irq_rx_ready(dev)) {
		uint8_t received_byte;
		uart_fifo_read(dev, &received_byte, 1);

		if (adcs_rx_len < ADCS_RX_BUF_SIZE) {
			adcs_rx_buf[adcs_rx_len++] = received_byte;
		}

		if (adcs_rx_len == adcs_rx_expected) {
			k_sem_give(&adcs_rx_sem);
		}
	}
}

adcs_rc_t adcs_init(void)
{
	if (!device_is_ready(adcs_uart)) {
		return ADCS_RC_ERR;
	}

	uart_irq_callback_set(adcs_uart, adcs_uart_isr);
	uart_irq_rx_enable(adcs_uart);

	return ADCS_RC_OK;
}

static bool adcs_verify_checksum(const uint8_t *buf, uint8_t len)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < len; ++i) {
		sum += buf[i];
	}

	return sum == 0;
}

adcs_rc_t adcs_get_id(uint8_t *id, uint8_t id_size)
{
	__ASSERT(id, "ID array cannot be NULL.");
	__ASSERT(id_size == ADCS_ID_SIZE, "ID array must be exactly %u bytes.", ADCS_ID_SIZE);

	/* Reset RX semaphore and count state */
	k_sem_reset(&adcs_rx_sem);
	adcs_rx_len = 0U;
	adcs_rx_expected = 4U + id_size + 1U;

	/* 5 byte command to read the ID. */
	uint8_t adcs_id_cmd[] = {0xc9, 0x01, 0x03, 0x00, 0x33};

	/* Send the command to read the ID. */
	for (size_t i = 0; i < sizeof(adcs_id_cmd); ++i) {
		uart_poll_out(adcs_uart, adcs_id_cmd[i]);
	}

	/* Wait for the ADCS to finish replying. */
	if (k_sem_take(&adcs_rx_sem, K_MSEC(1000)) != 0) {
		LOG_ERR("ADCS ID response timeout");
		return ADCS_RC_ERR;
	}

	if (adcs_rx_len < adcs_rx_expected) {
		LOG_ERR("ADCS ID response too short (%u bytes)", adcs_rx_len);
		return ADCS_RC_ERR;
	}

	uint8_t adcs_rx_i = 0;

	/* Verify the 4 byte RX header matches 4 byte command header. */
	for (; adcs_rx_i < 4; ++adcs_rx_i) {
		if (adcs_rx_buf[adcs_rx_i] != adcs_id_cmd[adcs_rx_i]) {
			return ADCS_RC_ERR;
		}
	}

	/* Output ID */
	for (uint8_t i = 0; i < id_size; ++i, ++adcs_rx_i) {
		if (adcs_rx_buf[adcs_rx_i] != adcs_id_expected[i]) {
			/* TODO (?): return bitmap of where the ID failed to match with expected */
			LOG_ERR("ADCS ID has unexpected value at position %u, 0x%02x", i,
				adcs_rx_buf[adcs_rx_i]);
			return ADCS_RC_ERR;
		}

		id[i] = adcs_rx_buf[adcs_rx_i];
	}

	/* Verify checksum. */
	if (!adcs_verify_checksum(adcs_rx_buf, adcs_rx_len)) {
		LOG_ERR("ADCS Checksum error");
		return ADCS_RC_ERR;
	}

	return ADCS_RC_OK;
}

adcs_rc_t adcs_get_stat(adcs_stat_t *status) {

	__ASSERT(status, "Status struct cannot be NULL");

	/* Reset RX semaphore and count state */
	k_sem_reset(&adcs_rx_sem);
	adcs_rx_len = 0U;
	adcs_rx_expected = 4U + 4U + 1U; /* stat reg is of type uint8[4] */

	uint8_t adcs_stat_cmd[] = {0xc9, 0x80, 0x01, 0x00, 0xB6}; /*Header, id, size, err, checksum for MTSR command packet by ADCS manual format*/

	/*Sending command*/
	for (size_t i = 0; i < sizeof(adcs_stat_cmd); ++i) {
		uart_poll_out(adcs_uart, adcs_stat_cmd[i]);

	}

	/*Waiting for ADCS response*/
	if (k_sem_take(&adcs_rx_sem, K_MSEC(1000)) != 0) {
		LOG_ERR("ADCS status register response timeout");
		return ADCS_RC_ERR;
	}

	if (adcs_rx_len < adcs_rx_expected) {
		LOG_ERR("ADCS status response too short (%u bytes)", adcs_rx_len);
		return ADCS_RC_ERR;
	}


	/*Check command echo for adcs_stat_cmd*/
	uint8_t adcs_rx_i = 0;
	for (; adcs_rx_i < 4; adcs_rx_i ++) {
		if (adcs_rx_buf[adcs_rx_i] != adcs_stat_cmd[adcs_rx_i]) {
			return ADCS_RC_ERR;
		}

	}

	/*Parse stat payload into adcs_stat_t*/
	uint8_t b0 = adcs_rx_buf[4]; /*blank to mode*/
	uint8_t b1 = adcs_rx_buf[5]; /*	cussv to tle*/
	/*uint8_t b2 = adcs_rx_buf[6] no important bits in this byte*/
	uint8_t b3 = adcs_rx_buf[7]; /*GNSS to HERR*/

	/*=======Load into status=======*/

	status->mode = (b0 >> 5) & 0x7; /*Bits 7 to 0*/

	/*bits 15 to 8*/
	status->tle = (b1 >> 7) & 0x1;
	status->des = (b1 >> 6) & 0x1;
	status->sun = (b1 >> 5) & 0x1;
	status->tgl = (b1 >> 4) & 0x1;
	status->tumb = (b1 >> 3) & 0x1;
	status->ame = (b1 >> 2) & 0x1;


	/*bits 31 to 24*/
	status->herr = (b3 >> 7) & 0x1;
	status->serr = (b3 >> 6) & 0x1;
	status->wdt = (b3 >> 5) & 0x1;
	status->uv = (b3 >> 4) & 0x1;
	status->oc = (b3 >> 3) & 0x1;
	status->ot = (b3 >> 2) & 0x1;
	status->gnss = b3 & 0x3; 

	/* Verify checksum. */
	if (!adcs_verify_checksum(adcs_rx_buf, adcs_rx_len)) {
		LOG_ERR("ADCS Checksum error");
		return ADCS_RC_ERR;
	}

	return ADCS_RC_OK;
	
}
