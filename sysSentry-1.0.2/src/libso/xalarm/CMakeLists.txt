# Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
# Description: cmake file for libxalarm
add_library(xalarm SHARED register_xalarm.c)
target_link_libraries(xalarm PRIVATE pthread securec)
set_target_properties(xalarm PROPERTIES LINK_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wtrampolines")
set_target_properties(xalarm PROPERTIES CMAKE_C_FLAGS "-shared -fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wall -Werror -g")
 
install(TARGETS xalarm LIBRARY DESTINATION ${XD_INSTALL_BINDIR})