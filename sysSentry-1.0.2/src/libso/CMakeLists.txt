# Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
# Description: cmake file for libxalarm

cmake_minimum_required(VERSION 3.12.1)
project(libxalarm)
 
SET(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

add_subdirectory(xalarm)
