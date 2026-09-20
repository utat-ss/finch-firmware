/*
 * Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FINCH_CSP_CSP_H_
#define FINCH_CSP_CSP_H_

int finch_csp_init(void);

#ifdef CONFIG_FINCH_CSP_CAN
int finch_csp_can_init(void);
#endif

#endif /* FINCH_CSP_CSP_H_ */
