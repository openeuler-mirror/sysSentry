# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# Description: cmake file for io_utils
cmake_minimum_required(VERSION 3.22)
project(sentry_io)
add_library(sentry_io SHARED io_utils.c)
set_target_properties(sentry_io PROPERTIES LINK_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
set_target_properties(sentry_io PROPERTIES COMPILE_FLAGS "-shared -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wall -Werror -Wtrampolines -g")
