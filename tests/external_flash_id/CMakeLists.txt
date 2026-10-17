# Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(external_flash_id_test)

target_sources(app PRIVATE src/main.c)
