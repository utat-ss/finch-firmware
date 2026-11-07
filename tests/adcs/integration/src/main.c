/*
 * Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <finch/adcs/adcs.h>

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(bench);

ZTEST_SUITE(basic_test, NULL, NULL, NULL, NULL, NULL);

ZTEST(basic_test, test_get_id)
{
	zassert_equal(adcs_init(), ADCS_RC_OK);

	uint8_t adcs_id[ADCS_ID_SIZE];
	zassert_equal(adcs_get_id(adcs_id, ADCS_ID_SIZE), ADCS_RC_OK);
}
