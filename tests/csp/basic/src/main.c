/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csp/csp.h>
#include <finch/csp/csp.h>
#include <zephyr/logging/log.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(csp_test);

static void *adcs_service_setup(void)
{
	int ret;

	ret = finch_csp_init();
	zassert_equal(ret, 0, "Unable to set up Finch CSP");

	return NULL;
}

ZTEST_SUITE(csp_tests, NULL, adcs_service_setup, NULL, NULL, NULL);

ZTEST(csp_tests, test_ping_reply)
{
    int res = csp_ping(0, 1000, 100, CSP_O_CRC32);
    zassert_false(res == -1);
}