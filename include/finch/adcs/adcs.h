/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief The Attitude Determination and Control System (ADCS) library
 *
 * The ADCS module controls the attitude of the satellite. The OBC communicates to the ADCS module
 * via UART. A typical workflow is asking the ADCS module the current attitude, checking if it is
 * nominal, asking it to correct the attitude if not nominal, and repeat.
 */

#ifndef FINCH_ADCS_H
#define FINCH_ADCS_H

#include <stdlib.h>
#include <stdint.h>

#define ADCS_ID_SIZE 12

typedef enum {
	ADCS_RC_OK,
	ADCS_RC_ERR,
} adcs_rc_t;

/**
 * @brief Initialize and enable the ADCS UART callback
 *
 * @retval ADCS_RC_OK	On success.
 * @retval ADCS_RC_ERR	On failure.
 */
adcs_rc_t adcs_init(void);

/**
 * @brief Get the ID of the ADCS module; blocking
 *
 * Note the ID of the dev board is different than the actual module.
 *
 * @param id    ID of the ADCS module.
 * @param size	Size of the ID array, MUST be ADCS_ID_SIZE.
 *
 * @retval ADCS_RC_OK   On success.
 * @retval ADCS_RC_ERR  On failure.
 */
adcs_rc_t adcs_get_id(uint8_t *id, uint8_t size);

#endif /* FINCH_ADCS_H */
