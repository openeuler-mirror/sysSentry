# Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
# Description: cmake file for log_utils
cmake_minimum_required(VERSION 3.22)
project(sentry_log)
add_library(sentry_log SHARED log_utils.c)
set(CMAKE_VERBOSE_MAKEFILE ON)
set_target_properties(sentry_log PROPERTIES LINK_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
set_target_properties(sentry_log PROPERTIES COMPILE_FLAGS "-shared -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wall -Werror -Wtrampolines -g")
