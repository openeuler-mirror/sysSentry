cmake_minimum_required(VERSION 3.14.1)
project(cat-cli C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
# 父子模块都可以直接<include>如下路径下的头文件
include_directories(PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/plugin/cpu_patrol)

# 导入子模块
add_subdirectory(plugin)

add_executable(cat-cli)
file(GLOB CLI_SRC *.c)
target_sources(cat-cli PUBLIC ${CLI_SRC})
# cpu_patrol以独立的so链接
TARGET_LINK_LIBRARIES(cat-cli pthread -ldl  cpu_patrol)

# 安装相关
install(TARGETS cat-cli
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
install(TARGETS cpu_patrol LIBRARY DESTINATION lib)

# 创建软链接
install(CODE "execute_process(COMMAND ln -sf ${CMAKE_INSTALL_PREFIX}/bin/cat-cli /usr/local/bin/cat-cli)")
install(CODE "execute_process(COMMAND ln -sf ${CMAKE_INSTALL_PREFIX}/lib/libcpu_patrol.so /lib64/libcpu_patrol.so)")

# 成功安装信息
install(CODE "MESSAGE(\"[INFO] install successfully, run 'cat-cli -h' to test\")")


