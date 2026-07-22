# Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
# Description: cmake file for libxalarm
add_library(xalarm SHARED register_xalarm.c)
target_link_libraries(xalarm PRIVATE pthread -ljson-c ${CMAKE_CURRENT_SOURCE_DIR}/../../libsentry/c/io/build/libsentry_io.so)
target_include_directories(xalarm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libsentry/c/io)
set_target_properties(xalarm PROPERTIES LINK_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
set_target_properties(xalarm PROPERTIES COMPILE_FLAGS "-shared -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wall -Werror -Wtrampolines -g")

install(TARGETS xalarm LIBRARY DESTINATION ${XD_INSTALL_BINDIR})
