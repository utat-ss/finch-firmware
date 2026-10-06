/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FINCH_ADCS_SERVICE_H
#define FINCH_ADCS_SERVICE_H

#include <finch/adcs/adcs.h>

enum adcs_service_cmd_type {
	ADCS_SERVICE_CMD_GET_ID = 0x01,
};

struct adcs_service_cmd {
    uint8_t type; /* enum adcs_service_cmd_type */
};

enum adcs_service_status {
	ADCS_SERVICE_OK = 0x00,
	ADCS_SERVICE_ERR_UNKNOWN_CMD = 0x01,
	ADCS_SERVICE_ERR_ADCS = 0x02,
};

struct adcs_service_res {
    uint8_t status; /* enum adcs_service_res_type */
    uint8_t cmd_type; /* enum adcs_service_cmd_type */
    union {
        uint8_t id[ADCS_ID_SIZE];
    } data;
};

int adcs_service_init(void);

#endif /* FINCH_ADCS_SERVICE_H */
