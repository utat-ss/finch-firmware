/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adcs_service.h"
#include <csp/csp.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <finch/adcs/adcs.h>
#include <string.h>

LOG_MODULE_REGISTER(adcs_service);

#define ADCS_CMD_PORT 10
#define ADCS_CMD_BACKLOG 5 // Justification needed
#define ADCS_READ_TIMEOUT_MS 1000
#define ADCS_CMD_GET_ID 0x01
#define ADCS_SERVICE_THREAD_PRIORITY 2

static csp_socket_t sock = {0};
static struct k_thread adcs_service_thread_data;
static k_tid_t adcs_service_tid;
K_THREAD_STACK_DEFINE(adcs_service_stack, 1024);

struct adcs_request {
	uint8_t cmd;
};

static void adcs_service_handler(csp_conn_t *conn)
{
	LOG_INF("Received ADCS command");
	struct adcs_request adcs_req;
	adcs_rc_t adcs_rc;

	/* For now, we assume a single packet for each command. */
	csp_packet_t *req = csp_read(conn, ADCS_READ_TIMEOUT_MS);
	if (req == NULL) {
		LOG_WRN("No request received on ADCS service port");
		csp_close(conn);
		return;
	}
	memcpy(&adcs_req, req->data, sizeof(struct adcs_request));
	csp_buffer_free(req);

	LOG_INF("ADCS CMD: [0x%X]", adcs_req.cmd);

	csp_packet_t *resp = csp_buffer_get(0);
	if (resp == NULL) {
		LOG_ERR("Failed to allocate ADCS response buffer");
		csp_close(conn);
		return;
	}

	switch (adcs_req.cmd) {
	case ADCS_CMD_GET_ID:
		adcs_rc = adcs_get_id(resp->data, ADCS_ID_SIZE);
		if (adcs_rc == ADCS_RC_ERR) {
			LOG_ERR("Failed to get ADCS ID (%d)", adcs_rc);
			csp_buffer_free(resp);
			goto out;
		}
		resp->length = ADCS_ID_SIZE;
		break;
	default:
		LOG_WRN("Unknown ADCS command [0x%X]", adcs_req.cmd);
		csp_buffer_free(resp);
		goto out;
	}

	// csp_send takes ownership of the packet and frees it
	csp_send(conn, resp);

out:

	csp_close(conn);
}

static void adcs_service_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Listening for ADCS Commands on port %u", ADCS_CMD_PORT);

	while (1) {
		csp_conn_t *conn = csp_accept(&sock, CSP_MAX_TIMEOUT);
		if (conn == NULL) {
			LOG_ERR("Failed to accept connection on ADCS service port %u", ADCS_CMD_PORT);
			continue;
		}

		adcs_service_handler(conn);
	}
}

int adcs_service_init(void)
{
	int err;

	err = csp_bind(&sock, ADCS_CMD_PORT);
	if (err != CSP_ERR_NONE) {
		LOG_ERR("Failed to bind ADCS service port %u (%d)", ADCS_CMD_PORT, err);
		return -EIO;
	}

	err = csp_listen(&sock, ADCS_CMD_BACKLOG);
	if (err != CSP_ERR_NONE) {
		LOG_ERR("Failed to listen on ADCS service port %u (%d)", ADCS_CMD_PORT, err);
		return -EIO;
	}

	adcs_service_tid = k_thread_create(
		&adcs_service_thread_data,
		adcs_service_stack,
		K_THREAD_STACK_SIZEOF(adcs_service_stack),
		adcs_service_thread,
		NULL, NULL, NULL,
		ADCS_SERVICE_THREAD_PRIORITY,
		0,
		K_NO_WAIT
	);

	if (adcs_service_tid == NULL) {
		LOG_ERR("Failed to create ADCS service thread");
		return -1;
	}

	return 0;
}
