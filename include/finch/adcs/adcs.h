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

extern uint8_t const ADCS_ID_SIZE;

typedef enum {
	ADCS_RC_OK,
	ADCS_RC_ERR,
} adcs_rc_t;


typedef struct {
	bool herr;
	bool serr;
	bool wdt;
	bool uv;
	bool oc;
	bool ot; 
	uint8_t gnss;
	bool tle;
	bool des;
	bool sun;
	bool tgl;
	bool tumb;
	bool ame;
	// uint8_t cussv; // Think this is one of the custom ones that we don't need?
	uint8_t mode;

} adcs_stat_t;

adcs_rc_t adcs_get_stat(adcs_stat_t *status);


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
