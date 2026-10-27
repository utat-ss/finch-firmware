# Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(csp_ping_test)

target_sources(app PRIVATE src/main.c)

# glibc only exposes the htobe32()-style helpers libcsp uses under _DEFAULT_SOURCE
zephyr_compile_definitions(_DEFAULT_SOURCE)
