/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csp/csp.h>
#include <csp/drivers/can_zephyr.h>

#if !DT_HAS_CHOSEN(zephyr_canbus)
#error "CONFIG_FINCH_CSP_CAN requires the board devicetree to set the zephyr,canbus chosen node"
#endif

#define CANBUS_NODE DT_CHOSEN(zephyr_canbus)

// Canbus
static const struct device *const can = DEVICE_DT_GET(CANBUS_NODE);
static const uint32_t can_bitrate = DT_PROP(CANBUS_NODE, bitrate);
static csp_iface_t *can_if;

int finch_csp_can_init(void)
{
	int csp_rc;

	// Initializes the canbus csp interface
	csp_rc = csp_can_open_and_add_interface(can, "CAN", CONFIG_FINCH_CSP_NODE_ADDRESS, can_bitrate, 0, 0, &can_if);

	return csp_rc;
}
