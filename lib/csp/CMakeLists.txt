# Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(src/core.c)
zephyr_library_sources_ifdef(CONFIG_FINCH_CSP_CAN src/can.c)
zephyr_library_link_libraries(csp)
