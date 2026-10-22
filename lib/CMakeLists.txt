# Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_FINCH_ADCS adcs)
add_subdirectory_ifdef(CONFIG_FINCH_ADCS_SERVICE adcs_service)
add_subdirectory_ifdef(CONFIG_FINCH_CCSDS123B ccsds123b)
add_subdirectory_ifdef(CONFIG_FINCH_CSP csp)
add_subdirectory_ifdef(CONFIG_FINCH_OBC_FLASH obc_flash)
add_subdirectory_ifdef(CONFIG_FINCH_OBC_LITTLEFS obc_littlefs)
