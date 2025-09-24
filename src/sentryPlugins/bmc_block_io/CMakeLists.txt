cmake_minimum_required (VERSION 3.12)
project(bmc_block_io)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_SKIP_INSTALL_RPATH TRUE)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../../libs/libxalarm
)
link_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/build/libxalarm
)

set(SOURCE src/main.cpp
            src/logger.cpp
            src/common.cpp
            src/cbmcblockio.cpp)

add_executable(bmc_block_io ${SOURCE})
target_link_libraries(bmc_block_io PRIVATE xalarm pthread json-c)
