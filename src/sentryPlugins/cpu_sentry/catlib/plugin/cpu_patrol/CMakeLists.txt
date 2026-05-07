project(cpu_patrol C)

set(CMAKE_SKIP_RPATH TRUE)

file(GLOB CPU_PATROL_SRC *.c)

add_library(cpu_patrol SHARED ${CPU_PATROL_SRC})

include_directories(${CMAKE_SOURCE_DIR}/../../../libs/libxalarm/)
link_directories(${CMAKE_SOURCE_DIR}/../../../libs/build/libxalarm/)

TARGET_LINK_LIBRARIES(cpu_patrol pthread xalarm -ldl)

