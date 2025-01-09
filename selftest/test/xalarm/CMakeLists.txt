# Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
cmake_minimum_required(VERSION 3.12.1)
project(xd_alarm_demo)

SET(CMAKE_VERBOSE_MAKEFILE OFF)

add_executable(reg_alarm_demo reg_demo.c)
add_executable(send_alarm_demo send_demo.c)
add_executable(unreg_alarm_demo unreg_demo.c)
add_executable(upgrade_alarm_demo upgrade_demo.c)
target_link_libraries(reg_alarm_demo xalarm)
target_link_libraries(send_alarm_demo xalarm)
target_link_libraries(unreg_alarm_demo xalarm)
target_link_libraries(upgrade_alarm_demo xalarm)
