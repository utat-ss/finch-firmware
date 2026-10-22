/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csp/csp.h>
#include <csp/interfaces/csp_if_lo.h>
#include <finch/csp/csp.h>
#include <finch/adcs_service/adcs_service.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define CONNECT_TIMEOUT 1000
#define READ_TIMEOUT 1000
#define INVALID_CMD 0xFF

static uint8_t const adcs_id_expected[ADCS_ID_SIZE] = {0x54, 0x41, 0x44, 0x31, 0x30, 0x32,
						       0x30, 0x35, 0x33, 0x44, 0x00, 0x00};

static csp_conn_t *conn;
static csp_packet_t *req_packet;
static csp_packet_t *res_packet;

static void *adcs_service_setup(void)
{
	int ret;

	ret = adcs_init();
	zassert_equal(ret, ADCS_RC_OK, "ADCS failed to initialize");

	ret = finch_csp_init();
	zassert_equal(ret, 0, "Unable to set up Finch CSP");

	ret = adcs_service_init();
	zassert_equal(ret, 0, "Unable to start adcs service");

	return NULL;
}

static void adcs_service_before(void *fixture)
{
	ARG_UNUSED(fixture);

	// Open connection
	conn = csp_connect(CSP_PRIO_NORM, csp_if_lo.addr, ADCS_SERVICE_PORT, CONNECT_TIMEOUT,
			   CSP_O_CRC32);
	zassert_not_null(conn, "Could not connect to node at loopback");

	// Get packet from buffer
	req_packet = csp_buffer_get(0);
	zassert_not_null(req_packet);
}

static void adcs_service_after(void *fixture)
{
	ARG_UNUSED(fixture);

	if (req_packet != NULL) {
		csp_buffer_free(req_packet);
		req_packet = NULL;
	}

	if (res_packet != NULL) {
		csp_buffer_free(res_packet);
		res_packet = NULL;
	}

	if (conn != NULL) {
		csp_close(conn);
		conn = NULL;
	}
}

ZTEST_SUITE(test_adcs_service, NULL, adcs_service_setup, adcs_service_before,
	    adcs_service_after, NULL);

ZTEST(test_adcs_service, get_id)
{
	struct adcs_service_cmd *cmd;
	struct adcs_service_res *res;

	// Construct packet to send
	req_packet->length = sizeof(struct adcs_service_cmd);
	cmd = (struct adcs_service_cmd *)req_packet->data;
	cmd->type = ADCS_SERVICE_CMD_GET_ID;

	// Send packet
	csp_send(conn, req_packet);
	req_packet = NULL;

	// Receive packet
	res_packet = csp_read(conn, READ_TIMEOUT);
	zassert_not_null(res_packet);

	// Assert resulting id packet is as expected
	zassert_equal(res_packet->length, sizeof(struct adcs_service_res));
	res = (struct adcs_service_res *)res_packet->data;
	zassert_equal(res->status, ADCS_SERVICE_OK);
	zassert_equal(res->cmd_type, ADCS_SERVICE_CMD_GET_ID);
	zassert_mem_equal(res->data.id, adcs_id_expected, ADCS_ID_SIZE);
}

ZTEST(test_adcs_service, invalid_cmd)
{
	struct adcs_service_cmd *cmd;
	struct adcs_service_res *res;

	// Construct packet to send
	req_packet->length = sizeof(struct adcs_service_cmd);
	cmd = (struct adcs_service_cmd *)req_packet->data;
	cmd->type = INVALID_CMD;

	// Send packet
	csp_send(conn, req_packet);
	req_packet = NULL;

	// Receive packet
	res_packet = csp_read(conn, READ_TIMEOUT);
	zassert_not_null(res_packet);

	// Assert error response packet is as expected
	zassert_equal(res_packet->length, sizeof(struct adcs_service_res));
	res = (struct adcs_service_res *)res_packet->data;
	zassert_equal(res->status, ADCS_SERVICE_ERR_UNKNOWN_CMD);
	zassert_equal(res->cmd_type, INVALID_CMD);
}

ZTEST(test_adcs_service, invalid_packet_length)
{
	struct adcs_service_res *res;

	// Construct packet with invalid length to send
	req_packet->length = sizeof(struct adcs_service_cmd) + 1;

	// Send packet
	csp_send(conn, req_packet);
	req_packet = NULL;

	// Receive packet
	res_packet = csp_read(conn, READ_TIMEOUT);
	zassert_not_null(res_packet);

	// Assert error response packet is as expected
	zassert_equal(res_packet->length, sizeof(struct adcs_service_res));
	res = (struct adcs_service_res *)res_packet->data;
	zassert_equal(res->status, ADCS_SERVICE_ERR_PACKET);
}
