#!/bin/bash

# Copyright (c) 2025 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

set -e

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../finch-firmware-env.sh"

cppcheck --enable=all \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    -i"${FINCH_FIRMWARE_ROOT}/build" \
    --error-exitcode=1 \
    "${FINCH_FIRMWARE_ROOT}"
