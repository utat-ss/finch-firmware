cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tests_adcs_dev_board)

include_directories(../../../include)

file(GLOB SOURCES src/*.c)

target_sources(app PRIVATE ${SOURCES})
