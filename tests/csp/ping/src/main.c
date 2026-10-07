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

LOG_MODULE_REGISTER(bench);

ZTEST_SUITE(csp_ping, NULL, NULL, NULL, NULL, NULL);

ZTEST(csp_ping, test_ping_reply)
{
	int csp_rc;

    csp_rc = finch_csp_init();

    zassert_equal(csp_rc, 0);

    LOG_INF("CSP Initialized.");

    int res = csp_ping(0, 1000, 100, CSP_O_CRC32);

    zassert_false(res == -1);
}